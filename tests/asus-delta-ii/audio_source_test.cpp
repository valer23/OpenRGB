/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "Controllers/AsusAuraUSBController/AsusDeltaIIController/AsusDeltaIIAudioSource_Windows.h"
#include <cassert>
#include <iostream>
using namespace std::chrono_literals;
int main()
{
    using Source=AsusDeltaIIAudioSource;
    using Endpoint=Source::Endpoint;
    const Endpoint target{"headphone","receiver",true,true};
    assert(Source::SelectEndpoint("receiver",{target})=="headphone");
    assert(Source::SelectEndpoint("",{target}).empty());
    assert(Source::SelectEndpoint("other",{target}).empty());
    assert(Source::SelectEndpoint("receiver",{{"microphone","receiver",false,true}}).empty());
    assert(Source::SelectEndpoint("receiver",{{"inactive","receiver",true,false}}).empty());
    assert(Source::SelectEndpoint("receiver",{{"","receiver",true,true}}).empty());
    assert(Source::SelectEndpoint("receiver",{target,target}).empty());
    assert(Source::SelectEndpoint("receiver",{target,{"other","elsewhere",true,true}})=="headphone");
    Source source;std::string error;
    assert(source.Generation()==0);
    assert(!source.Read().available);
    assert(!source.Open(Source::Clock::now()-5s,error));
    assert(error=="Fresh explicit Delta II awake confirmation required for audio observation");
    assert(!source.Open(Source::Clock::now()+60s,error));
    assert(source.Generation()==0 && !source.Read().available);
    source.Stop();
    std::cout<<"Delta II audio source: unique receiver-render selection, ambiguous/mic/inactive rejection, closed startup and stale/future awake rejection passed\n";
}