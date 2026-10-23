QT += core gui widgets
CONFIG += c++17 plugin release rtti
CONFIG -= debug_and_release
TEMPLATE = lib
TARGET = OpenRGBBatteryStatusPlugin
INCLUDEPATH += ../.. ../../dependencies/json ../../RGBController
HEADERS += BatteryStatusPlugin.h BatteryStatusPresentation.h ../../OpenRGBBatteryStatusInterface.h
SOURCES += BatteryStatusPlugin.cpp
DESTDIR = .

# qmake does not infer Q_PLUGIN_METADATA's JSON dependency on this toolchain.
battery_metadata.target = moc_BatteryStatusPlugin.cpp
battery_metadata.depends = $$PWD/BatteryStatusPlugin.json
QMAKE_EXTRA_TARGETS += battery_metadata
