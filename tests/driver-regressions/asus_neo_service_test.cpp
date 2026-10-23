/*---------------------------------------------------------*\
| asus_neo_service_test.cpp                                |
|                                                         |
| ASUS service XML tests; never activates the ASUS service  |
|                                                         |
| valer23                                   23 Oct 2026    |
|                                                         |
| This file is part of the OpenRGB project                 |
| SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/
/* Standalone Windows test. Include the implementation to exercise its internal
 * XML boundary without adding test-only production entry points. Only MSXML6
 * is activated; AsusAuraNeoSaveViaService is deliberately never called.
 * g++ -std=c++17 -Istubs asus_neo_service_test.cpp -lole32 -loleaut32 -luuid
 *     -ladvapi32 -o asus_neo_service_test.exe
 */
#include <cassert>
#include <iostream>
unsigned int logged_errors = 0;
#include "../../Controllers/AsusAuraUSBController/AsusAuraNeoController/AsusAuraNeoService_Windows.cpp"

std::wstring Output(unsigned int index)
{
    return L"<led key='" + std::to_wstring(index) + L"'><DeviceName>Mainboard_Master</DeviceName>"
           L"<type>65536</type><color>255</color><hue>0</hue><saturation>1</saturation><lightness>0.5</lightness>"
           L"<hashue2>0</hashue2><hue2>0</hue2><speed>" + std::to_wstring(index + 7) +
           L"</speed><direction>" + std::to_wstring(index + 11) + L"</direction></led>";
}

std::wstring Fixture()
{
    const std::wstring mode = L"<mode key='5'>" + Output(0) + Output(1) + L"</mode>";
    return L"<root><header>ASUS_AURA</header><version>1.2</version><device key='Group'>"
           L"<scene><state>S0</state><mode key='100'><preserve>live-colour</preserve></mode></scene>"
           L"<scene><state>Non-S0</state>" + mode + L"</scene></device><device key='Mainboard'>"
           L"<scene><state>S0</state><mode key='1'><preserve>independent-live</preserve></mode></scene>"
           L"<scene><state>Non-S0</state>" + mode + L"</scene></device></root>";
}

std::wstring Replace(std::wstring text, const std::wstring& from, const std::wstring& to)
{
    const auto pos = text.find(from);
    assert(pos != std::wstring::npos);
    text.replace(pos, from.size(), to);
    return text;
}

COM<IXMLDOMDocument2> Saved(const ServiceRequest& request)
{
    auto cache = Parse(Fixture());
    auto payload = Parse(request.xml);
    auto requested = Select(payload.get(), MAINBOARD_MODE);
    for(const wchar_t* path : {MAINBOARD_MODE, GROUP_MODE})
    {
        auto old = Select(cache.get(), path);
        IXMLDOMNode* parent_raw = nullptr;
        old->get_parentNode(&parent_raw);
        COM<IXMLDOMNode> parent(parent_raw);
        /* MSXML permits cross-document nodes in replaceChild and transfers
         * ownership, so parse separately for each replacement. */
        auto copy_document = Parse(XML(requested.get()));
        auto copy = Select(copy_document.get(), L"/mode");
        IXMLDOMNode* removed = nullptr;
        assert(SUCCEEDED(parent->replaceChild(copy.get(), old.get(), &removed)));
        if(removed) removed->Release();
    }
    return cache;
}

int main()
{
    assert(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)));
    {
        assert(!Parse(L"<root>"));
        assert(!Parse(L"<!DOCTYPE root [<!ENTITY x 'injected'>]><root>&x;</root>"));
        assert(!Parse(std::wstring(MAX_PROFILE_SIZE + 1, L'x')));
        auto cache = Parse(Fixture());
        assert(cache);
        const auto before = XML(cache.get());
        ServiceRequest request;
        assert(BuildRequest(cache.get(), 0, 0xA600FF, request));
        assert(request.mode == 101 && request.color == 0);
        assert(request.hue == 0 && request.saturation == 0 && request.lightness == 0);
        assert(XML(cache.get()) == before);
        auto payload = Parse(request.xml);
        assert(Text(payload.get(), L"/root/funcid") == L"4");
        assert(!Select(payload.get(), LIVE_SCENE));
        assert(!Select(payload.get(), L"/root/device/scene[state='S0']"));
        assert(!Select(payload.get(), L"/root/device[@key='Group']"));
        auto mode = Select(payload.get(), MAINBOARD_MODE);
        assert(MainboardOutputs(mode.get()));
        assert(Text(mode.get(), L"led[@key='0']/speed") == L"7");
        assert(Text(mode.get(), L"led[@key='1']/direction") == L"12");
        assert(Text(mode.get(), L"iscolorsynced") == L"0" && Text(mode.get(), L"singlecolor") == L"1");
        auto after = Saved(request);
        assert(Verified(after.get(), request));
        assert(PutText(after.get(), Select(after.get(), L"/root/device[@key='Mainboard']/scene[state='S0']/mode").get(), L"preserve", L"changed"));
        assert(!Verified(after.get(), request));
        after = Saved(request);
        assert(PutText(after.get(), Select(after.get(), GROUP_MODE).get(), L"@key", L"5"));
        assert(!Verified(after.get(), request));

        for(unsigned int effect : {1u, 2u, 3u, 4u, 5u, 0x0Du})
        {
            assert(BuildRequest(cache.get(), effect, 0xA600FF, request));
            assert(request.mode == effect && request.color == 0xFF00A6);
            assert(std::abs(request.hue - (4.0 + 166.0 / 255) / 6) < 0.000001);
            assert(request.saturation == 1 && request.lightness == 0.5);
            after = Saved(request);
            assert(Verified(after.get(), request));
            assert(PutText(after.get(), Select(after.get(), MAINBOARD_MODE).get(), L"led[@key='0']/color", L"0"));
            assert(!Verified(after.get(), request));
            for(const wchar_t* field : {L"led[@key='0']/hashue2", L"led[@key='1']/hue2"})
            {
                after = Saved(request);
                assert(PutText(after.get(), Select(after.get(), MAINBOARD_MODE).get(), field, L"1"));
                assert(!Verified(after.get(), request));
            }
        }
        for(unsigned int rgb : {0u, 0xFFFFFFu, 0x808080u, 0xFF0000u, 0x00FF00u, 0x0000FFu})
        {
            assert(BuildRequest(cache.get(), 1, rgb, request));
            after = Saved(request);
            assert(Verified(after.get(), request));
            assert(std::isfinite(request.hue) && std::isfinite(request.saturation));
        }
        assert(!BuildRequest(cache.get(), 6, 0, request));
        assert(!BuildRequest(cache.get(), 1, 0x1000000, request));
        for(const auto& xml : {
            Replace(Fixture(), L"Mainboard_Master", L"AddressableStrip"),
            Replace(Fixture(), L"<type>65536</type>", L"<type>69632</type>"),
            Replace(Fixture(), L"key='Mainboard'", L"key='OtherBoard'"),
            Replace(Fixture(), L"<header>ASUS_AURA</header>", L"<header>Other</header>")})
        {
            auto invalid = Parse(xml);
            /* The first two replacements affect Group only; BuildRequest must
             * not use that unrelated route as its request source. */
            if(xml.find(L"key='OtherBoard'") != std::wstring::npos || xml.find(L"<header>Other") != std::wstring::npos)
                assert(!BuildRequest(invalid.get(), 0, 0, request));
            else
            {
                assert(BuildRequest(invalid.get(), 0, 0, request));
                assert(!Verified(invalid.get(), request));
            }
        }
        const std::wstring prefix = L"<root><header>ASUS_AURA</header><version>1.2</version><device key='Group'><scene><state>S0</state></scene></device><device key='Mainboard'><scene><state>Non-S0</state><mode key='5'>";
        const std::wstring suffix = L"</mode></scene></device></root>";
        for(const auto& outputs : {Output(0), Output(0) + Output(0), Output(0) + Output(2),
             Replace(Output(0), L"65536", L"69632") + Output(1),
             Replace(Output(0), L"Mainboard_Master", L"AddressableStrip") + Output(1)})
        {
            auto invalid = Parse(prefix + outputs + suffix);
            assert(invalid && !BuildRequest(invalid.get(), 0, 0, request));
        }
    }
    CoUninitialize();
    std::cout << "ASUS service XML tests passed; no ASUS COM server or hardware invoked\n";
}
