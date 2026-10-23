/*---------------------------------------------------------*\
| AsusAuraNeoService_Windows.cpp                           |
|                                                         |
| ASUS Aura NEO explicit shutdown save through ASUS service |
|                                                         |
| valer23                                   23 Oct 2026    |
|                                                         |
| This file is part of the OpenRGB project                 |
| SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/
#include "AsusAuraNeoService_Windows.h"
#include "LogManager.h"
#include <windows.h>
#include <msxml2.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <locale>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{
const wchar_t* SERVICE_CLASS = L"CLSID\\{95775DC4-77AA-4E94-8CF6-68267EEF1856}\\LocalServer32";
const CLSID SERVICE_CLSID = {0x95775dc4, 0x77aa, 0x4e94, {0x8c, 0xf6, 0x68, 0x26, 0x7e, 0xef, 0x18, 0x56}};
const size_t MAX_PROFILE_SIZE = 1024 * 1024;
const wchar_t* MAINBOARD_MODE = L"/root/device[@key='Mainboard']/scene[state='Non-S0']/mode";
const wchar_t* GROUP_MODE = L"/root/device[@key='Group']/scene[state='Non-S0']/mode";
const wchar_t* LIVE_SCENE = L"/root/device[@key='Group']/scene[state='S0']";

class ReleaseCOM
{
public:
    template<class T> void operator()(T* p) const
    {
        if(p)
        {
            p->Release();
        }
    }
};
template<class T> using COM = std::unique_ptr<T, ReleaseCOM>;
class StringBSTR
{
public:
    BSTR value = nullptr;
    explicit StringBSTR(const std::wstring& text) : value(SysAllocStringLen(text.data(), (UINT)text.size())) {}
    ~StringBSTR() { SysFreeString(value); }
    StringBSTR() = default;
    StringBSTR(const StringBSTR&) = delete;
    operator BSTR() const { return value; }
    std::wstring text() const { return value ? std::wstring(value, SysStringLen(value)) : L""; }
};

COM<IXMLDOMNode> Select(IXMLDOMNode* parent, const wchar_t* path)
{
    IXMLDOMNode* node = nullptr;
    if(parent)
    {
        parent->selectSingleNode(StringBSTR(path), &node);
    }
    return COM<IXMLDOMNode>(node);
}

std::wstring Text(IXMLDOMNode* parent, const wchar_t* path)
{
    COM<IXMLDOMNode> node = Select(parent, path);
    StringBSTR text;
    if(node)
    {
        node->get_text(&text.value);
    }
    return text.text();
}

std::wstring XML(IXMLDOMNode* node)
{
    StringBSTR text;
    if(node)
    {
        node->get_xml(&text.value);
    }
    return text.text();
}

std::wstring LiveScenes(IXMLDOMNode* root)
{
    IXMLDOMNodeList* raw = nullptr;
    if(FAILED(root->selectNodes(StringBSTR(L"/root/device/scene[state='S0']"), &raw)) || !raw) return L"";
    COM<IXMLDOMNodeList> scenes(raw);
    long count = 0;
    if(FAILED(scenes->get_length(&count))) return L"";
    std::wstring snapshot;
    for(long i = 0; i < count; ++i)
    {
        IXMLDOMNode* node = nullptr;
        if(FAILED(scenes->get_item(i, &node)) || !node) return L"";
        COM<IXMLDOMNode> scene(node);
        snapshot += Text(scene.get(), L"../@key") + L"\n" + XML(scene.get()) + L"\n";
    }
    return snapshot;
}

COM<IXMLDOMDocument2> Parse(const std::wstring& text)
{
    IXMLDOMDocument2* raw = nullptr;
    if(text.size() > MAX_PROFILE_SIZE || FAILED(CoCreateInstance(__uuidof(DOMDocument60), nullptr,
        CLSCTX_INPROC_SERVER, __uuidof(IXMLDOMDocument2), (void**)&raw))) return {};
    COM<IXMLDOMDocument2> doc(raw);
    VARIANT prohibit;
    VariantInit(&prohibit);
    prohibit.vt = VT_BOOL;
    prohibit.boolVal = VARIANT_TRUE;
    VARIANT_BOOL loaded = VARIANT_FALSE;
    if(FAILED(doc->put_async(VARIANT_FALSE)) || FAILED(doc->put_validateOnParse(VARIANT_FALSE)) ||
       FAILED(doc->put_resolveExternals(VARIANT_FALSE)) ||
       FAILED(doc->setProperty(StringBSTR(L"ProhibitDTD"), prohibit)) ||
       FAILED(doc->loadXML(StringBSTR(text), &loaded)) || loaded != VARIANT_TRUE) return {};
    return doc;
}

bool PutText(IXMLDOMDocument2* owner, IXMLDOMNode* parent, const wchar_t* name, const std::wstring& value)
{
    IXMLDOMNodeList* raw = nullptr;
    if(FAILED(parent->selectNodes(StringBSTR(name), &raw)) || !raw) return false;
    COM<IXMLDOMNodeList> nodes(raw);
    long count = 0;
    if(FAILED(nodes->get_length(&count)) || count > 1) return false;
    COM<IXMLDOMNode> node = Select(parent, name);
    if(!node)
    {
        IXMLDOMElement* element = nullptr;
        if(FAILED(owner->createElement(StringBSTR(name), &element)) || !element) return false;
        node.reset(element);
        IXMLDOMNode* appended = nullptr;
        HRESULT hr = parent->appendChild(node.get(), &appended);
        if(appended) appended->Release();
        if(FAILED(hr)) return false;
    }
    return SUCCEEDED(node->put_text(StringBSTR(value)));
}

bool MainboardOutputs(IXMLDOMNode* mode)
{
    if(!mode) return false;
    IXMLDOMNodeList* raw = nullptr;
    if(FAILED(mode->selectNodes(StringBSTR(L"led"), &raw)) || !raw) return false;
    COM<IXMLDOMNodeList> leds(raw);
    long count = 0;
    if(FAILED(leds->get_length(&count)) || count != 2) return false;
    for(const wchar_t* path : {L"led[@key='0']", L"led[@key='1']"})
    {
        COM<IXMLDOMNode> led = Select(mode, path);
        if(!led || Text(led.get(), L"DeviceName") != L"Mainboard_Master" ||
           Text(led.get(), L"type") != L"65536" || !Select(led.get(), L"speed") ||
           !Select(led.get(), L"direction")) return false;
    }
    return true;
}

struct ServiceRequest
{
    std::wstring xml, live;
    std::wstring speed[2], direction[2];
    unsigned int mode = 0, color = 0;
    double hue = 0, saturation = 0, lightness = 0;
};

std::wstring Number(double value)
{
    std::wostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::fixed << std::setprecision(6) << value;
    return stream.str();
}

bool BuildRequest(IXMLDOMDocument2* cache, unsigned int effect, unsigned int rgb, ServiceRequest& request)
{
    if((effect > 5 && effect != 0x0D) || rgb > 0xFFFFFF) return false;
    COM<IXMLDOMNode> mode = Select(cache, MAINBOARD_MODE);
    COM<IXMLDOMNode> live = Select(cache, LIVE_SCENE);
    if(!MainboardOutputs(mode.get()) || !live || Text(cache, L"/root/header") != L"ASUS_AURA" ||
       Text(cache, L"/root/version") != L"1.2") return false;
    request = ServiceRequest();
    request.mode = effect ? effect : 101;
    request.live = LiveScenes(cache);
    const unsigned int red = effect ? (rgb >> 16) & 255 : 0;
    const unsigned int green = effect ? (rgb >> 8) & 255 : 0;
    const unsigned int blue = effect ? rgb & 255 : 0;
    request.color = red | (green << 8) | (blue << 16);
    const double r = red / 255.0, g = green / 255.0, b = blue / 255.0;
    const double maximum = std::max({r, g, b}), minimum = std::min({r, g, b});
    const double delta = maximum - minimum;
    request.lightness = (maximum + minimum) / 2;
    if(delta > 0)
    {
        request.saturation = delta / (1 - std::abs(2 * request.lightness - 1));
        request.hue = maximum == r ? (g - b) / delta + (g < b ? 6 : 0) :
                      maximum == g ? (b - r) / delta + 2 : (r - g) / delta + 4;
        request.hue /= 6;
    }
    IXMLDOMNode* cloned = nullptr;
    if(FAILED(mode->cloneNode(VARIANT_TRUE, &cloned)) || !cloned) return false;
    COM<IXMLDOMNode> copy(cloned);
    if(!PutText(cache, copy.get(), L"@key", std::to_wstring(request.mode)) ||
       !PutText(cache, copy.get(), L"iscolorsynced", L"0") || !PutText(cache, copy.get(), L"singlecolor", L"1")) return false;
    for(unsigned int i = 0; i < 2; ++i)
    {
        COM<IXMLDOMNode> led = Select(copy.get(), i ? L"led[@key='1']" : L"led[@key='0']");
        request.speed[i] = Text(led.get(), L"speed");
        request.direction[i] = Text(led.get(), L"direction");
        if(!PutText(cache, led.get(), L"color", std::to_wstring(request.color)) ||
           !PutText(cache, led.get(), L"hue", Number(request.hue)) ||
           !PutText(cache, led.get(), L"saturation", Number(request.saturation)) ||
           !PutText(cache, led.get(), L"lightness", Number(request.lightness)) ||
           !PutText(cache, led.get(), L"hashue2", L"0") || !PutText(cache, led.get(), L"hue2", L"0")) return false;
    }
    request.xml = L"<root><header>ASUS_AURA</header><version>1.2</version><funcid>4</funcid>"
                  L"<device key='Mainboard'><scene><state>Non-S0</state>" + XML(copy.get()) + L"</scene></device></root>";
    return !request.live.empty() && (bool)Parse(request.xml);
}

bool Near(const std::wstring& text, double expected)
{
    std::wistringstream stream(text);
    stream.imbue(std::locale::classic());
    double value;
    return (stream >> value) && stream.peek() == std::char_traits<wchar_t>::eof() &&
           std::isfinite(value) && std::abs(value - expected) < 0.00002;
}

bool Verified(IXMLDOMDocument2* cache, const ServiceRequest& request)
{
    COM<IXMLDOMNode> live = Select(cache, LIVE_SCENE);
    if(!live || LiveScenes(cache) != request.live) return false;
    for(const wchar_t* path : {MAINBOARD_MODE, GROUP_MODE})
    {
        COM<IXMLDOMNode> mode = Select(cache, path);
        if(!MainboardOutputs(mode.get()) || Text(mode.get(), L"@key") != std::to_wstring(request.mode)) return false;
        for(unsigned int i = 0; i < 2; ++i)
        {
            COM<IXMLDOMNode> led = Select(mode.get(), i ? L"led[@key='1']" : L"led[@key='0']");
            if(Text(led.get(), L"color") != std::to_wstring(request.color) ||
               !Near(Text(led.get(), L"hue"), request.hue) || !Near(Text(led.get(), L"saturation"), request.saturation) ||
               !Near(Text(led.get(), L"lightness"), request.lightness) ||
               !Near(Text(led.get(), L"hashue2"), 0) || !Near(Text(led.get(), L"hue2"), 0) ||
               Text(led.get(), L"speed") != request.speed[i] || Text(led.get(), L"direction") != request.direction[i]) return false;
        }
    }
    return true;
}

bool ServiceRunning()
{
    SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    SC_HANDLE service = manager ? OpenServiceW(manager, L"LightingService", SERVICE_QUERY_STATUS) : nullptr;
    SERVICE_STATUS status = {};
    bool running = service && QueryServiceStatus(service, &status) && status.dwCurrentState == SERVICE_RUNNING;
    if(service) CloseServiceHandle(service);
    if(manager) CloseServiceHandle(manager);
    return running;
}

std::wstring ProfilePath()
{
    wchar_t value[32768] = {};
    DWORD bytes = sizeof(value);
    if(RegGetValueW(HKEY_CLASSES_ROOT, SERVICE_CLASS, L"ServerExecutable", RRF_RT_REG_SZ | RRF_SUBKEY_WOW6432KEY,
        nullptr, value, &bytes) != ERROR_SUCCESS)
    {
        bytes = sizeof(value);
        if(RegGetValueW(HKEY_CLASSES_ROOT, SERVICE_CLASS, nullptr, RRF_RT_REG_SZ | RRF_SUBKEY_WOW6432KEY,
            nullptr, value, &bytes) != ERROR_SUCCESS || value[0] != L'"') return L"";
        std::wstring quoted(value + 1);
        const std::wstring::size_type end = quoted.find(L'"');
        if(end == std::wstring::npos) return L"";
        quoted.resize(end);
        std::copy(quoted.begin(), quoted.end(), value);
        value[quoted.size()] = 0;
    }
    std::wstring executable(value);
    const std::wstring::size_type separator = executable.find_last_of(L"\\/");
    if(separator == std::wstring::npos || _wcsicmp(executable.substr(separator + 1).c_str(), L"LightingService.exe")) return L"";
    return executable.substr(0, separator + 1) + L"LastProfile.xml";
}

COM<IXMLDOMDocument2> ReadProfile(const std::wstring& path)
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if(file == INVALID_HANDLE_VALUE) return {};
    struct CloseFile
    {
        HANDLE handle;
        ~CloseFile() { CloseHandle(handle); }
    } close{file};
    LARGE_INTEGER size = {};
    std::vector<char> bytes;
    DWORD read = 0;
    bool valid = GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart <= (LONGLONG)MAX_PROFILE_SIZE;
    if(valid)
    {
        bytes.resize((size_t)size.QuadPart);
        valid = ReadFile(file, bytes.data(), (DWORD)bytes.size(), &read, nullptr) && read == bytes.size();
    }
    if(!valid) return {};
    if(bytes.size() >= 2 && (unsigned char)bytes[0] == 0xFF && (unsigned char)bytes[1] == 0xFE)
        return bytes.size() % 2 ? COM<IXMLDOMDocument2>() : Parse(std::wstring((const wchar_t*)(bytes.data() + 2), (bytes.size() - 2) / 2));
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), (int)bytes.size(), nullptr, 0);
    if(count <= 0) return {};
    std::wstring text(count, 0);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), (int)bytes.size(), &text[0], count);
    return Parse(text);
}
}

bool AsusAuraNeoSaveViaService(unsigned int effect_id, unsigned int rgb_rrggbb) try
{
    if(!ServiceRunning())
    {
        LOG_ERROR("[ASUS Aura NEO] ASUS shutdown save requires LightingService already running; no settings written");
        return false;
    }
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if(FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
    {
        LOG_ERROR("[ASUS Aura NEO] ASUS shutdown COM initialization failed (0x%08lx); no settings written", (unsigned long)initialized);
        return false;
    }
    struct Uninitialize { bool needed; ~Uninitialize() { if(needed) CoUninitialize(); } } uninitialize{SUCCEEDED(initialized)};
    const std::wstring path = ProfilePath();
    COM<IXMLDOMDocument2> before = path.empty() ? COM<IXMLDOMDocument2>() : ReadProfile(path);
    ServiceRequest request;
    if(!before || !BuildRequest(before.get(), effect_id, rgb_rrggbb, request))
    {
        LOG_ERROR("[ASUS Aura NEO] ASUS cached motherboard shutdown route unavailable or unsupported; no settings written");
        return false;
    }
    IDispatch* raw = nullptr;
    HRESULT hr = CoCreateInstance(SERVICE_CLSID, nullptr, CLSCTX_LOCAL_SERVER | CLSCTX_ACTIVATE_32_BIT_SERVER,
                                   IID_IDispatch, (void**)&raw);
    COM<IDispatch> service(raw);
    if(FAILED(hr) || !service)
    {
        LOG_ERROR("[ASUS Aura NEO] ASUS shutdown service connection failed (0x%08lx); no settings written", (unsigned long)hr);
        return false;
    }
    VARIANT arguments[2];
    VariantInit(&arguments[0]); VariantInit(&arguments[1]);
    arguments[0].vt = arguments[1].vt = VT_BSTR;
    arguments[0].bstrVal = SysAllocStringLen(request.xml.data(), (UINT)request.xml.size());
    arguments[1].bstrVal = SysAllocString(L"Group");
    DISPID property = DISPID_PROPERTYPUT;
    DISPPARAMS parameters = {arguments, &property, 2, 1};
    EXCEPINFO exception = {};
    hr = arguments[0].bstrVal && arguments[1].bstrVal ? service->Invoke(1, IID_NULL, LOCALE_INVARIANT,
        DISPATCH_PROPERTYPUT, &parameters, nullptr, &exception, nullptr) : E_OUTOFMEMORY;
    VariantClear(&arguments[0]); VariantClear(&arguments[1]);
    SysFreeString(exception.bstrSource); SysFreeString(exception.bstrDescription); SysFreeString(exception.bstrHelpFile);
    if(FAILED(hr))
    {
        LOG_ERROR("[ASUS Aura NEO] ASUS shutdown save failed (0x%08lx); settings may have partially changed", (unsigned long)hr);
        return false;
    }
    /* This bounds only file verification, not the synchronous vendor COM call. */
    const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    do
    {
        COM<IXMLDOMDocument2> after = ReadProfile(path);
        if(after && Verified(after.get(), request))
        {
            LOG_INFO("[ASUS Aura NEO] ASUS service saved shutdown settings; cached motherboard/group readback matched and cached S0 scenes stayed unchanged. Power-off persistence requires physical verification");
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } while(std::chrono::steady_clock::now() < deadline);
    LOG_ERROR("[ASUS Aura NEO] ASUS service accepted shutdown save but cached readback did not match; settings may have changed. No retry write or rollback attempted");
    return false;
}
catch(...)
{
    LOG_ERROR("[ASUS Aura NEO] ASUS shutdown save could not complete; settings may have changed. No fallback or rollback attempted");
    return false;
}
