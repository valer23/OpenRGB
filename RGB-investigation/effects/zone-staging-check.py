"""Compile the real ControllerZone colour callbacks against a tiny fake device."""
import pathlib
import subprocess
import sys

source, effects, compiler, output = map(pathlib.Path, sys.argv[1:])
text = (source / 'ControllerZone.cpp').read_text()
methods = []
for name in ('SetAllZoneLEDs', 'SetLED'):
    start = text.index('void ControllerZone::' + name + '(')
    end = text.index('\n}', start) + 2
    methods.append(text[start:end])
output.mkdir(parents=True, exist_ok=True)
program = r'''
#include "EffectFrameBatch.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#define REQUIRE(c) do {if(!(c)){std::cerr << "Failed: " << #c << '\n';return 1;}}while(false)
struct Device:RGBControllerInterface {
    std::map<unsigned int,RGBColor> values;
    unsigned int updates=0,bulk=0;
    void SetColor(unsigned int i,RGBColor color) override {values[i]=color;}
    void UpdateLEDs() override {++updates;}
    void SetAllZoneColors(unsigned int,RGBColor color){++bulk;for(unsigned int i=5;i<9;i++)values[i]=color;}
    std::string GetName(){return "fake";}
};
struct ColorUtils { static RGBColor apply_adjustments(RGBColor color,float,int,int){return color;} };
struct FrameTiming {static bool Enabled(){return false;}static void Add(const std::string&,long long){};};
struct ControllerZone {
    bool is_segment=false;unsigned int zone_idx=0,self_brightness=100;Device* controller;
    unsigned int leds_count(){return 4;}unsigned int start_idx(){return 5;}
    void SetAllZoneLEDs(RGBColor,int,int,int);void SetLED(int,RGBColor,int,int,int);
};
''' + '\n'.join(methods) + r'''
int main(){
    Device device;ControllerZone zone;zone.controller=&device;
    zone.SetAllZoneLEDs(7,100,0,0);REQUIRE(device.bulk==1);device.values.clear();
    {EffectFrameQueue queue;EffectFrameBatch frame;
    zone.SetAllZoneLEDs(11,100,0,0);zone.SetLED(2,12,100,0,0);
    REQUIRE(device.values.empty());REQUIRE(device.updates==0);REQUIRE(device.bulk==1);
    frame.Submit(queue,[](RGBControllerInterface* d,const EffectDeviceFrame& colors){for(auto e:colors)d->SetColor(e.first,e.second);d->UpdateLEDs();});}
    REQUIRE(device.values.size()==4);REQUIRE(device.values[5]==11);REQUIRE(device.values[7]==12);REQUIRE(device.updates==1);
    REQUIRE(!EffectSupportsPartialZones("Stack"));REQUIRE(!EffectSupportsPartialZones("GradientWave"));
    REQUIRE(!EffectSupportsPartialZones("SpectrumCycling"));REQUIRE(EffectSupportsPartialZones("RainbowWave"));
    std::cout << "Real whole-zone callbacks stage and submit once; stock assignment order is preserved\n";
}
'''
cpp = output / 'zone-staging.cpp'
exe = output / 'zone-staging.exe'
cpp.write_text(program)
subprocess.run([str(compiler), '-std=c++17', '-I' + str(effects / 'plugin'),
                '-I' + str(effects / 'test-stubs/frame-batch'), str(cpp), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
