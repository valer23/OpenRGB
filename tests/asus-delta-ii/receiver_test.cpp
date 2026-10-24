/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "Controllers/AsusAuraUSBController/AsusDeltaIIController/AsusDeltaIIReceiver.h"
#include <array>
#include <cassert>
#include <future>
#include <iostream>
struct FakeIo final : AsusDeltaIIReceiver::Io
{
    bool ready = true, allow_open = true, opened = false;
    int opens = 0, closes = 0, transfers = 0, result = 64;
    bool Ready() override { return ready; }
    bool Open(std::string& error) override
    {
        ++opens;
        if(!allow_open) { error = "Competing reader"; return false; }
        opened = true;
        return true;
    }
    void Close() override { if(opened) { ++closes; opened = false; } }
    int Transfer(bool, unsigned char*, unsigned int, unsigned int) override
    { ++transfers; return result; }
};
int main()
{
    FakeIo io;
    AsusDeltaIIReceiver receiver(io);
    std::string error;
    assert(io.opens == 0 && io.transfers == 0); // Construction is passive.
    {
        auto access = receiver.Acquire(error);
        assert(access && io.opens == 1 && io.transfers == 0);
        // A simultaneous manual battery or RGB request refuses instead of
        // opening another handle or waiting indefinitely behind an operation.
        auto other = std::async(std::launch::async, [&] {
            std::string busy;
            auto second = receiver.Acquire(busy);
            return !second && !busy.empty();
        });
        assert(other.get());
        assert(io.opens == 1);
        access.KeepOpen(true);
    }
    assert(io.closes == 0);
    {
        auto battery = receiver.Acquire(error);
        assert(battery && io.opens == 1); // Borrow retained RGB connection.
        std::array<unsigned char, 64> bytes = {};
        assert(battery.Transfer(false, bytes.data(), bytes.size(), 100) == 64);
    }
    assert(io.closes == 0 && io.transfers == 1);
    {
        auto access = receiver.Acquire(error);
        access.KeepOpen(false);
    }
    assert(io.closes == 1);
    io.allow_open = false;
    { auto access = receiver.Acquire(error); assert(!access && error == "Competing reader"); }
    assert(io.transfers == 1);
    io.allow_open = true;
    io.ready = false;
    int attempts = io.opens;
    { auto access = receiver.Acquire(error); assert(!access); }
    assert(io.opens == attempts); // Pending cancellation prevents a new reader.
    io.ready = true;
    {
        auto access = receiver.Acquire(error);
        assert(access);
        access.KeepOpen(true);
        io.result = -1;
        std::array<unsigned char, 64> bytes = {};
        assert(access.Transfer(true, bytes.data(), bytes.size(), 100) == -1);
        assert(!access); // Failed channel cannot be used for more commands.
        int sent = io.transfers;
        assert(access.Transfer(true, bytes.data(), bytes.size(), 100) == -1);
        assert(io.transfers == sent);
    }
    assert(io.closes == 2); // Failed retained connections are discarded.
    io.result = 64;
    std::promise<void> acquired, release_finished;
    auto resume = release_finished.get_future();
    auto in_flight = std::async(std::launch::async, [&] {
        std::string detail;
        auto transaction = receiver.Acquire(detail);
        assert(transaction);
        transaction.KeepOpen(true);
        acquired.set_value();
        resume.wait();
        // A late setup callback must not re-retain or write the released session.
        transaction.KeepOpen(true);
        std::array<unsigned char, 64> data = {};
        assert(!transaction);
        assert(transaction.Transfer(true, data.data(), data.size(), 100) == -1);
    });
    acquired.get_future().wait();
    int sent_before_release = io.transfers;
    auto releasing = std::async(std::launch::async, [&] { receiver.Release(); });
    assert(releasing.wait_for(std::chrono::milliseconds(200)) == std::future_status::ready);
    releasing.get(); // Must not wait behind the held transaction.
    release_finished.set_value();
    in_flight.get();
    assert(io.transfers == sent_before_release && io.closes == 3);
    {
        auto transaction = receiver.Acquire(error);
        assert(transaction);
        transaction.KeepOpen(true);
    }
    receiver.Release(); // Also closes an idle retained handle immediately.
    assert(io.closes == 4);
    std::cout << "Delta II single-owner receiver: sharing, contention, cancellation and failure cleanup passed\n";
}
