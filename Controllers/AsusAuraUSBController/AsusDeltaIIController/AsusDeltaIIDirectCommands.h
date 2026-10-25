/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "AsusDeltaIIProtocol.h"
#include "AsusDeltaIIReceiver.h"
#include <algorithm>
#include <chrono>
#include <functional>
#include <utility>
#include <vector>

// Direct-backend command path. Serialized by its owning controller/worker.
// Construction/discovery/configuration never opens a receiver or sends commands.
// Awake confirmation is a runtime user action, never a persisted setting.
class AsusDeltaIIDirectCommands
{
public:
    using Clock = std::chrono::steady_clock;
    using Time = Clock::time_point;
    using RGB = AsusDeltaIIProtocol::RGB;
    using Packet = AsusDeltaIIProtocol::Packet;
    explicit AsusDeltaIIDirectCommands(AsusDeltaIIReceiver& receiver,
                                      std::function<Time()> clock = Clock::now) :
        receiver(receiver), now(std::move(clock)) {}
    ~AsusDeltaIIDirectCommands() { Stop(); }
    AsusDeltaIIDirectCommands(const AsusDeltaIIDirectCommands&) = delete;
    AsusDeltaIIDirectCommands& operator=(const AsusDeltaIIDirectCommands&) = delete;

    bool ApplyBuiltin(unsigned mode, unsigned brightness, RGB colour,
                      Time confirmed_awake, std::string& error)
    {
        const auto setting = AsusDeltaIIProtocol::Builtin(mode, brightness, colour[0], colour[1], colour[2]);
        if(!setting) { error = "Invalid Delta II firmware effect or brightness"; return false; }
        return Setup({AsusDeltaIIProtocol::SoftwareMode(false), AsusDeltaIIProtocol::Power(true), *setting},
                     confirmed_awake, false, error);
    }
    bool ApplyOff(Time confirmed_awake, std::string& error)
    {
        return Setup({AsusDeltaIIProtocol::SoftwareMode(false), AsusDeltaIIProtocol::Power(false)},
                     confirmed_awake, false, error);
    }
    bool BeginSoftware(unsigned brightness, RGB colour, Time confirmed_awake, std::string& error)
    {
        const auto setting = AsusDeltaIIProtocol::Builtin(1, brightness, colour[0], colour[1], colour[2]);
        if(!setting) { error = "Invalid Delta II software brightness"; return false; }
        std::array<RGB, 14> initial;
        initial.fill(colour);
        return Setup({AsusDeltaIIProtocol::Power(true), *setting, AsusDeltaIIProtocol::SoftwareMode(true), AsusDeltaIIProtocol::SoftwareFrame(initial)},
                     confirmed_awake, true, error);
    }
    // Only the qualified audio policy may supply this short-lived permission.
    // No permission is inferred from USB presence, battery data or earlier frames.
    bool WriteFrame(RGB colour, Time expires, std::string& error)
    {
        error.clear();
        const auto current = now();
        if(!software_active || expires <= current || expires-current > std::chrono::milliseconds(250))
        {
            error = "Delta II software output has no current permission";
            return false;
        }
        auto transaction = receiver.AcquireExisting(error, retained_session);
        if(!transaction) { software_active = false; return false; }
        if(!Drain(transaction, expires, error)) { software_active = false; owns_retained_session = false; transaction.KeepOpen(false); return false; }
        std::array<RGB,14> colours;
        colours.fill(colour);
        auto packet = AsusDeltaIIProtocol::SoftwareFrame(colours);
        // Recheck after draining and immediately before the bounded write.
        if(now() >= expires) { error = "Delta II frame permission expired"; return false; }
        if(transaction.Transfer(true, packet.data(), packet.size(), 250) != 64)
        {
            error = "Delta II software write failed; no retry";
            software_active = false;
            owns_retained_session = false;
            transaction.KeepOpen(false);
            return false;
        }
        return true; // Captured frame30 has no matching acknowledgement.
    }
    void Stop()
    {
        software_active = false;
        if(owns_retained_session) { owns_retained_session = false; receiver.Release(); }
        // No reset, mode restore, final frame or other wake-producing cleanup.
    }
private:
    bool Fresh(Time confirmed) const
    {
        const auto current = now();
        return confirmed <= current && current-confirmed < std::chrono::seconds(5);
    }
    bool Input(AsusDeltaIIReceiver::Transaction& transaction, Packet& reply,
               unsigned timeout, std::string& error)
    {
        const int count = transaction.Transfer(false, reply.data(), reply.size(), timeout);
        if(count < 0) { error = "Delta II input failed"; return false; }
        if(count && AsusDeltaIIProtocol::ParseLinkState(reply.data(), count) == AsusDeltaIIProtocol::LinkState::Disconnected)
        {
            error = "Delta II headset disconnected; no further RGB writes";
            return false;
        }
        input_size = count;
        return true;
    }
    bool Drain(AsusDeltaIIReceiver::Transaction& transaction, Time deadline, std::string& error)
    {
        const auto until = std::min(deadline, now()+std::chrono::milliseconds(200));
        while(now() < until)
        {
            Packet reply{};
            if(!Input(transaction, reply, 10, error)) { return false; }
            if(input_size == 0 && now() < deadline) { return true; }
        }
        error = "Delta II input did not quiesce before permission expired";
        return false;
    }
    bool Setup(const std::vector<Packet>& packets, Time confirmed, bool retain, std::string& error)
    {
        error.clear();
        if(!Fresh(confirmed)) { error = "Fresh explicit Delta II awake confirmation required"; return false; }
        Stop();
        auto transaction = receiver.Acquire(error);
        if(!transaction) { return false; }
        transaction.KeepOpen(false);
        const auto deadline = confirmed+std::chrono::seconds(5);
        for(auto packet : packets)
        {
            if(!Drain(transaction, deadline, error)) { return false; }
            if(!Fresh(confirmed)) { error = "Delta II awake confirmation expired"; return false; }
            if(transaction.Transfer(true, packet.data(), packet.size(), 250) != 64)
            {
                error = "Delta II RGB command failed; no retry";
                return false;
            }
            if(packet[2] == 0x30) { continue; }
            const auto ack_until = std::min(deadline, now()+std::chrono::milliseconds(750));
            bool acknowledged = false;
            while(now() < ack_until)
            {
                Packet reply{};
                if(!Input(transaction, reply, 50, error)) { return false; }
                if(AsusDeltaIIProtocol::Acknowledged(packet, reply.data(), input_size)) { acknowledged = true; break; }
            }
            if(!acknowledged) { error = "Matching Delta II RGB acknowledgement missing; no retry"; return false; }
        }
        if(!transaction || !Fresh(confirmed)) { error = "Delta II awake setup expired or was cancelled"; return false; }
        retained_session = transaction.Session();
        transaction.KeepOpen(retain);
        software_active = retain && static_cast<bool>(transaction);
        owns_retained_session = software_active;
        if(!transaction) { error = "Delta II awake setup was cancelled"; return false; }
        return true;
    }
    AsusDeltaIIReceiver& receiver;
    std::function<Time()> now;
    bool software_active = false;
    bool owns_retained_session = false;
    uint64_t retained_session = 0;
    int input_size = 0;
};
