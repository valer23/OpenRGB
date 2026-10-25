#include "Controllers/CorsairPeripheralV2Controller/CorsairStartupRecoveryPolicy.h"
#include <cassert>
#include <future>
#include <iostream>
using namespace std::chrono_literals;
int main(){
 using Policy=CorsairStartupRecoveryPolicy;
 Policy policy;const Policy::Time zero{};
 assert(!policy.Input("receiver",zero,zero));
 policy.Watch("control","receiver");
 assert(!policy.Input("other",zero,zero));
 assert(!policy.Input("receiver",zero,zero+1000ms));
 assert(!policy.Input("receiver",zero+1ms,zero));
 auto first=policy.Input("receiver",zero,zero);assert(first);
 assert(policy.Current(*first,zero));
 assert(!policy.Input("receiver",zero+1ms,zero+1ms));
 policy.Finished(*first,false);
 assert(!policy.Input("receiver",zero+1000ms,zero+1000ms));
 auto second=policy.Input("receiver",zero+2000ms,zero+2000ms);assert(second);
 assert(!policy.Current(*first,zero+2000ms));
 assert(!policy.Current(*second,zero+3000ms));
 policy.Clear();assert(!policy.Current(*second,zero+2000ms));
 policy.Watch("old-path","receiver");
 auto old=policy.Input("receiver",zero+4000ms,zero+4000ms);assert(old);
 policy.Watch("new-path","receiver"); // Same physical receiver re-enumerated.
 assert(!policy.Current(*old,zero+4000ms));
 auto current=policy.Input("receiver",zero+4001ms,zero+4001ms);assert(current && current->path=="new-path");
 policy.Finished(*current,true);assert(policy.Empty());
 policy.Watch("reused-path","old-container");
 auto reused=policy.Input("old-container",zero+6000ms,zero+6000ms);assert(reused);
 policy.Watch("reused-path","new-container");
 assert(!policy.Current(*reused,zero+6000ms));
 assert(!policy.Input("old-container",zero+6001ms,zero+6001ms));
 assert(policy.Input("new-container",zero+6001ms,zero+6001ms));
 policy.Clear();
 // Try-scheduling cannot wait behind detection, replace a queued scan, or run during shutdown.
 std::mutex mutex;std::function<void()> slot;int ran=0;
 std::promise<void> held,release;auto resume=release.get_future();
 auto worker=std::async(std::launch::async,[&](){std::lock_guard<std::mutex> lock(mutex);held.set_value();resume.wait();});
 held.get_future().wait();
 auto scheduling=std::async(std::launch::async,[&](){return TryScheduleCorsairRecovery(mutex,slot,true,[&](){++ran;});});
 const bool nonblocking=scheduling.wait_for(200ms)==std::future_status::ready;
 release.set_value();worker.get();
 assert(nonblocking && !scheduling.get());
 slot=[&](){ran=10;};assert(!TryScheduleCorsairRecovery(mutex,slot,true,[&](){++ran;}));slot();assert(ran==10);
 slot={};assert(!TryScheduleCorsairRecovery(mutex,slot,false,[&](){++ran;}));
 assert(TryScheduleCorsairRecovery(mutex,slot,true,[&](){++ran;}));slot();assert(ran==11);
 assert(CorsairMouseInputAge(5,0xfffffff0U)==21); // Message tick wrap.
 assert(CorsairMouseInputMeaningful(1,0,0) && CorsairMouseInputMeaningful(0,0,1));
 assert(!CorsairMouseInputMeaningful(0,0,0));
 // Exercise the same worker decision path, including time/identity loss DURING metadata lookup.
 for(int scenario=0;scenario<7;++scenario)
 {
  bool current=true,registered=false,enabled=true;int metadata=0,queries=0,profileLoads=0;
  if(scenario==0)current=false;
  if(scenario==1)registered=true;
  if(scenario==2)enabled=false;
  const auto recovered=RunCorsairRecoveryAttempt([&](){return current;},[&](){return registered;},[&](){return enabled;},
   [&](){++metadata;if(scenario==3 || scenario==4)current=false;return scenario!=5;},
   [&](){++queries;registered=true;++profileLoads;});
  if(scenario<3)assert(metadata==0 && queries==0);
  if(scenario==3 || scenario==4 || scenario==5)assert(metadata==1 && queries==0 && !recovered);
  if(scenario==6)assert(metadata==1 && queries==1 && profileLoads==1 && recovered);
 }
 policy.Watch("stale-control","same-receiver");policy.ForgetReceiver("same-receiver");assert(policy.Empty());
 assert(!CorsairRecoveryInputScope::Requested());
 {
  CorsairRecoveryInputScope outer([](){return true;});
  assert(CorsairRecoveryInputScope::Requested() && CorsairRecoveryInputScope::Current());
  CorsairRecoveryInputScope::MarkAmbiguous();assert(CorsairRecoveryInputScope::Ambiguous());
  {CorsairRecoveryInputScope empty(std::function<bool()>{});assert(!CorsairRecoveryInputScope::Requested());}
  assert(CorsairRecoveryInputScope::Current() && CorsairRecoveryInputScope::Ambiguous());
 }
 assert(!CorsairRecoveryInputScope::Requested());
 std::cout<<"Corsair startup recovery: exact source, fresh input, cooldown, replug, cleanup, busy/occupied scheduler and empty reports passed\n";
}
