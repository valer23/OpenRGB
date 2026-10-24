/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

// One receiver owner in the core, shared by RGB and manual battery reads.
// Acquiring a transaction opens the exact collection but never sends commands.
class AsusDeltaIIReceiver
{
public:
    struct Io
    {
        virtual ~Io() = default;
        virtual bool Ready() = 0;
        virtual bool Open(std::string& error) = 0;
        virtual void Close() = 0;
        virtual int Transfer(bool write, unsigned char* data, unsigned int size, unsigned int timeout) = 0;
    };
    class Transaction
    {
    public:
        Transaction(AsusDeltaIIReceiver& receiver, std::string& error) :
            owner(receiver), generation(receiver.generation.load()), lock(receiver.mutex, std::try_to_lock)
        {
            error.clear();
            if(!lock.owns_lock())
            {
                error = "Delta II receiver is busy with another OpenRGB operation; no new reader opened";
                return;
            }
            if(!owner.io.Ready())
            {
                error = "Previous Delta II I/O cancellation is pending; no new reader opened";
                return;
            }
            if(!owner.open)
            {
                if(!owner.io.Open(error)) { owner.io.Close(); return; }
                owner.open = true;
            }
            valid = true;
        }
        ~Transaction()
        {
            if(lock.owns_lock() && owner.open && (!owner.keep_open.load() || failed || generation != owner.generation.load()))
            {
                owner.io.Close();
                owner.open = false;
                owner.keep_open = false;
            }
        }
        Transaction(const Transaction&) = delete;
        Transaction& operator=(const Transaction&) = delete;
        explicit operator bool() const { return valid && generation == owner.generation.load(); }
        void KeepOpen(bool enabled) { if(*this) { owner.keep_open = enabled; } }
        int Transfer(bool write, unsigned char* data, unsigned int size, unsigned int timeout)
        {
            if(!*this) { return -1; }
            const int result = owner.io.Transfer(write, data, size, timeout);
            if(result < 0) { valid = false; failed = true; }
            return result;
        }
    private:
        AsusDeltaIIReceiver& owner;
        uint64_t generation;
        std::unique_lock<std::mutex> lock;
        bool valid = false;
        bool failed = false;
    };
    explicit AsusDeltaIIReceiver(Io& channel) : io(channel) {}
    AsusDeltaIIReceiver(const AsusDeltaIIReceiver&) = delete;
    AsusDeltaIIReceiver& operator=(const AsusDeltaIIReceiver&) = delete;
    Transaction Acquire(std::string& error) { return Transaction(*this, error); }
    // Invalidate in-flight transactions immediately. Closing waits only for the
    // current bounded operation to leave its transaction; the caller never waits.
    // Call outside a transaction, e.g. on disconnect or backend teardown.
    void Release()
    {
        ++generation;
        keep_open = false;
        std::unique_lock<std::mutex> lock(mutex, std::try_to_lock);
        if(lock.owns_lock() && open)
        {
            io.Close();
            open = false;
        }
    }
private:
    Io& io;
    std::mutex mutex;
    bool open = false;
    std::atomic<bool> keep_open{false};
    std::atomic<uint64_t> generation{0};
};
#ifdef _WIN32
AsusDeltaIIReceiver& GetAsusDeltaIIReceiver();
#endif
