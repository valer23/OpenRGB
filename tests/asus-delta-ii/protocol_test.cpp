/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "Controllers/AsusAuraUSBController/AsusDeltaIIController/AsusDeltaIIProtocol.h"
#include <nlohmann/json.hpp>
#include <cassert>
#include <fstream>
#include <iostream>
#include <vector>
using AsusDeltaIIProtocol::Packet;
int main()
{
    std::ifstream file("tests/asus-delta-ii/fixtures/builtin-lighting-20261024.json");
    assert(file.good());
    nlohmann::json capture; file >> capture;
    for(const auto& sample : capture.at("samples"))
    {
        const auto request=sample.at("request").get<std::vector<unsigned char>>();
        const auto reply=sample.at("reply").get<std::vector<unsigned char>>();
        assert(sample.at("request_bytes")==64 && sample.at("reply_bytes")==64);
        Packet expected={}; std::copy(request.begin(),request.end(),expected.begin());
        const auto encoded=request[2]==0x10 ? AsusDeltaIIProtocol::Power(false) :
            *AsusDeltaIIProtocol::Builtin(request[5],request[6],request[7],request[8],request[9]);
        assert(encoded==expected);
        Packet ack={}; std::copy(reply.begin(),reply.end(),ack.begin());
        assert(AsusDeltaIIProtocol::Acknowledged(encoded,ack.data(),ack.size()));
        for(size_t length=0;length<64;++length)
            assert(!AsusDeltaIIProtocol::Acknowledged(encoded,ack.data(),length));
        ack[1]=0x12; assert(!AsusDeltaIIProtocol::Acknowledged(encoded,ack.data(),ack.size()));
        ack[1]=0x51; ack[2]^=1; assert(!AsusDeltaIIProtocol::Acknowledged(encoded,ack.data(),ack.size()));
        ack[2]^=1; ack[5]=1; assert(!AsusDeltaIIProtocol::Acknowledged(encoded,ack.data(),ack.size()));
    }
    assert(!AsusDeltaIIProtocol::Builtin(0,100,0,0,0)); // Off has its own command.
    assert(!AsusDeltaIIProtocol::Builtin(6,100,0,0,0)); // Undocumented on this receiver.
    assert(!AsusDeltaIIProtocol::Builtin(1,101,0,0,0));
    assert(!AsusDeltaIIProtocol::Builtin(257,100,0,0,0)); // Validate before narrowing.
    auto on=AsusDeltaIIProtocol::Power(true);
    assert(on[0]==0xCC && on[1]==0x51 && on[2]==0x10 && on[5]==1);
    assert(!AsusDeltaIIProtocol::Acknowledged(on,nullptr,64));
    Packet invalid={}; assert(!AsusDeltaIIProtocol::Acknowledged(invalid,invalid.data(),64));
    std::ifstream software_file("tests/asus-delta-ii/fixtures/software-lighting-20261024.json");
    assert(software_file.good());
    nlohmann::json software; software_file >> software;
    Packet enabled = AsusDeltaIIProtocol::SoftwareMode(true);
    for(const auto& sample : software.at("samples"))
    {
        const auto data = sample.at("data").get<std::vector<unsigned char>>();
        assert(data.size() == 64);
        Packet expected = {}; std::copy(data.begin(), data.end(), expected.begin());
        if(sample.at("endpoint") == "0x00")
        {
            if(data[2] == 0x33)
                assert(AsusDeltaIIProtocol::SoftwareMode(data[5] == 1) == expected);
            else
            {
                std::array<AsusDeltaIIProtocol::RGB, 14> colours = {};
                colours.fill({data[5], data[6], data[7]});
                const auto frame = AsusDeltaIIProtocol::SoftwareFrame(colours);
                assert(frame == expected);
                assert(!AsusDeltaIIProtocol::Acknowledged(frame, frame.data(), frame.size()));
            }
        }
        else if(data[1] == 0x51)
            assert(AsusDeltaIIProtocol::Acknowledged(enabled, data.data(), data.size()));
        else
            assert(!AsusDeltaIIProtocol::Acknowledged(enabled, data.data(), data.size()));
    }
    // Exercise byte placement without claiming independent LEDs were validated.
    std::array<AsusDeltaIIProtocol::RGB, 14> distinct = {};
    distinct.front() = {1, 2, 3}; distinct.back() = {4, 5, 6};
    auto frame = AsusDeltaIIProtocol::SoftwareFrame(distinct);
    assert(frame[5] == 1 && frame[6] == 2 && frame[7] == 3);
    assert(frame[44] == 4 && frame[45] == 5 && frame[46] == 6);
    assert(std::all_of(frame.begin() + 47, frame.end(), [](unsigned char b) { return b == 0; }));
    std::cout<<"Delta II captured built-in and software commands: encoding and acknowledgement routing passed\n";
}
