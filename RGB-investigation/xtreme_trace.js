'use strict';
// Observe vendor calls. Never replace functions, alter arguments or call a GPU API.
const installed = new Set();
let recording = false;
let eventCount = 0;
function log(event) { if (eventCount++ < 10000) send(event); }
function hex(address, size) {
    if (size < 0 || size > 256 || address.isNull()) return null;
    try { return Array.from(new Uint8Array(address.readByteArray(size)), b => b.toString(16).padStart(2, '0')).join(''); }
    catch (_) { return null; }
}
function attach(address, name, callbacks) {
    if (!address || address.isNull() || installed.has(address.toString())) return;
    installed.add(address.toString());
    try { Interceptor.attach(address, callbacks); log({kind:'hook', name, address:address.toString()}); }
    catch (e) { log({kind:'hook-error', name, error:String(e)}); }
}
const ids = new Map([[0xE812EB07,'I2CWrite'], [0x2FDE12C5,'I2CRead'],
                     [0x283AC65A,'I2CWriteEx'], [0x4D7B0709,'I2CReadEx']]);
function i2cSnapshot(p) {
    try {
        // Target executable is x86. Validate structure size from version.
        const version = p.readU32(), length = version & 0xffff;
        if (Process.pointerSize !== 4 || length < 28 || length > 128) return {version, raw:hex(p, Math.min(length,128))};
        const regSize = p.add(16).readU32(), dataSize = p.add(24).readU32();
        return {version, deviceAddress:p.add(9).readU8(), regSize, dataSize,
                register:hex(p.add(12).readPointer(),regSize),
                data:hex(p.add(20).readPointer(),dataSize), raw:hex(p,length)};
    } catch (e) { return {error:String(e)}; }
}
function hookI2C(address,name) {
    attach(address,name,{
        onEnter(args) { this.active=recording; if (!this.active) return;
            this.info=args[1]; this.before=i2cSnapshot(this.info); },
        onLeave(retval) { if(this.active) log({kind:'i2c',name,status:retval.toInt32(),
            before:this.before,after:i2cSnapshot(this.info)}); }
    });
}
function hookQuery(address,name) {
    attach(address,name,{
        onEnter(args) { this.id=args[0].toUInt32(); },
        onLeave(retval) {
            log({kind:'resolve', resolver:name, id:'0x'+this.id.toString(16), address:retval.toString()});
            if(ids.has(this.id)) hookI2C(retval,ids.get(this.id));
        }
    });
}
const observer = Process.attachModuleObserver({onAdded(module) {
    const name = module.name.toLowerCase();
    if (name === 'hid.dll') {
        for (const api of ['HidD_SetFeature','HidD_SetOutputReport']) {
            attach(module.findExportByName(api),api,{
                onEnter(args) {
                    const caller=Process.findModuleByAddress(this.returnAddress);
                    this.active=recording && caller !== null &&
                        ['devicedll.dll','xtreme tuner.exe'].includes(caller.name.toLowerCase());
                    if(this.active) this.report={handle:args[0].toString(),length:args[2].toUInt32(),
                        payload:hex(args[1],Math.min(args[2].toUInt32(),256)),caller:caller.name};
                },
                onLeave(retval) { if(this.active) log({kind:'hid-output',name:api,...this.report,result:retval.toInt32()}); }
            });
        }
    }
    if (name === 'nvapi.dll' || name === 'nvapi64.dll') {
        log({kind:'module',name:module.name,base:module.base.toString()});
        hookQuery(module.findExportByName('nvapi_QueryInterface'),'nvapi_QueryInterface');
        const direct=module.findExportByName('nvapi_Direct_GetMethod');
        attach(direct,'nvapi_Direct_GetMethod',{
            onEnter(args) { this.values=[args[0].toString(),args[1].toString(),args[2].toString()]; },
            onLeave(retval) { log({kind:'direct-resolve',args:this.values,result:retval.toString()}); }
        });
    }
    if (name === 'devicedll.dll') {
        for(const exp of module.enumerateExports()) {
            if(exp.type !== 'function' || !exp.name.includes('LED')) continue;
            attach(exp.address,exp.name,{
                onEnter(args) { this.active=recording; if(this.active) {
                    this.values=[]; for(let i=0;i<6;i++) this.values.push(args[i].toString()); } },
                onLeave(retval) { if(this.active) log({kind:'vendor-led',name:exp.name,args:this.values,result:retval.toString()}); }
            });
        }
    }
}});
rpc.exports = { mark(label) { recording=true; log({kind:'marker',label}); return true; },
                stop() { recording=false; return eventCount; } };
send({kind:'ready',arch:Process.arch,pid:Process.id});
