"""Exact captured KFA2 lighting replay. Defaults to read-only inspection."""
import argparse
import ctypes as c
from pathlib import Path
import json
import time

U32 = c.c_uint32
U8 = c.c_uint8
HANDLE = c.c_void_p


class NVAPIError(RuntimeError):
    def __init__(self,status,name):
        self.status=status
        super().__init__(f'{name} failed with NVAPI status {status}')


class I2CInfo(c.Structure):
    _fields_ = [('version',U32),('display_mask',U32),('is_ddc',U8),('address',U8),
                ('register',c.POINTER(U8)),('register_size',U32),
                ('data',c.POINTER(U8)),('size',U32),('speed',U32),
                ('speed_khz',U32),('port_id',U8),('is_port_set',U32)]


def captured_sequences():
    path=Path(__file__).parent/'Xtreme-lighting-20261011-175205.jsonl'
    result={}
    label=None
    for line in path.read_text().splitlines():
        e=json.loads(line)['event']
        if e.get('kind')=='marker': label=e['label'];result[label]=[]
        if e.get('kind')=='i2c' and e['name']=='I2CWrite' and e['status']==0:
            p=e['before']
            if p['deviceAddress']==0x46:result[label].append((int(p['register'],16),bytes.fromhex(p['data'])))
    expected={
        'GREEN':[(2,b'\x00\xff\x00'),(5,b'\x00'),(6,b'\x20'),(8,b'\x5a')],
        'BLUE':[(2,b'\x00\x00\xff'),(5,b'\x00'),(6,b'\x20'),(8,b'\x5a')],
        'RED_AGAIN':[(2,b'\xff\x00\x00'),(5,b'\x00'),(6,b'\x20'),(8,b'\x5a')],
        'MBSYNC_ON':[(7,b'\x01')],
    }
    for label,values in expected.items():
        if result.get(label)!=values:raise RuntimeError(f'Unexpected captured {label} sequence')
    if (7,b'\x00') not in result['RED']:raise RuntimeError('No captured MBSync OFF command')
    return expected


class GPU:
    def __init__(self):
        # Use the installed system NVIDIA DLL, not a DLL in the workspace.
        self.dll=c.CDLL('C:/Windows/System32/nvapi64.dll')
        self.query=self.dll.nvapi_QueryInterface
        self.query.argtypes=[U32];self.query.restype=HANDLE
        init=self.function(0x0150E828)
        self.check(init(),'NvAPI initialise')
        enum=self.function(0xE5AC921F,c.POINTER(HANDLE),c.POINTER(c.c_int32))
        pci=self.function(0x2DDFB66E,HANDLE,*([c.POINTER(U32)]*4))
        handles=(HANDLE*64)();count=c.c_int32()
        self.check(enum(handles,c.byref(count)),'GPU enumeration')
        matches=[]
        for i in range(count.value):
            device,sub,revision,extended=U32(),U32(),U32(),U32()
            self.check(pci(handles[i],c.byref(device),c.byref(sub),c.byref(revision),c.byref(extended)),'PCI identity')
            print(f'GPU {i}: device={device.value:08X}, subsystem={sub.value:08X}',flush=True)
            if (device.value,sub.value)==(0x220810DE,0x153510DE):matches.append(handles[i])
        if len(matches)!=1:raise RuntimeError('Expected exactly one confirmed KFA2 GPU; refusing access')
        self.handle=matches[0]
        self.read_api=self.function(0x2FDE12C5,HANDLE,c.POINTER(I2CInfo))
        self.write_api=self.function(0xE812EB07,HANDLE,c.POINTER(I2CInfo))

    def function(self,identifier,*args):
        address=self.query(identifier)
        if not address:raise RuntimeError(f'NVAPI interface unavailable: {identifier:08X}')
        return c.CFUNCTYPE(c.c_int32,*args)(address)

    @staticmethod
    def check(status,name):
        if status!=0:raise NVAPIError(status,name)

    def transfer(self,register,data,write=False):
        reg=(U8*1)(register)
        buffer=(U8*len(data))(*data)
        info=I2CInfo()
        info.version=c.sizeof(info)|(3<<16)
        info.display_mask=1
        info.is_ddc=2  # Preserve vendor value; do not normalise this byte.
        info.address=0x46  # NVAPI wire address; 7-bit I2C address is 0x23.
        info.register=reg;info.register_size=1
        info.data=buffer;info.size=len(data)
        info.speed=0;info.speed_khz=0;info.port_id=1;info.is_port_set=1
        self.check((self.write_api if write else self.read_api)(self.handle,c.byref(info)),f'{"Write" if write else "Read"} lighting register {register:02X}')
        return bytes(buffer)

    def read(self,register,count=1):return self.transfer(register,bytes(count))
    def write(self,register,data):
        if register not in (2,5,6,7,8):raise RuntimeError('Outside verified lighting registers')
        self.transfer(register,data,True)
        time.sleep(0.025)

    def set_sync(self,value):
        if value not in (b'\x00',b'\x01'):raise ValueError('Invalid captured sync value')
        # Allow the preceding Apply to finish. NVAPI -1 is generic, not a
        # proven busy indication; retry only this idempotent lighting flag.
        time.sleep(0.2)
        last_error=None
        for attempt in range(3):
            try:
                self.write(7,value)
                actual=self.read(7)
                if actual!=value:raise RuntimeError(f'MBSync readback {actual.hex()} differs from {value.hex()}')
                print(f'MBSync restored/selected: {actual.hex()} (verified by readback).',flush=True)
                return
            except NVAPIError as error:
                if error.status!=-1:raise
                last_error=error
                print(f'MBSync attempt {attempt+1} returned -1; allowing a short pause.',flush=True)
                time.sleep(0.2*(attempt+1))
        raise last_error

    def restore(self,snapshot):
        errors=[]
        # Keep attempting independent restoration steps if one write fails.
        for reg,key in ((2,'rgb'),(5,'mode'),(6,'mode2')):
            try:self.write(reg,snapshot[key])
            except RuntimeError as error:errors.append(str(error))
        try:self.write(8,b'\x5a')
        except RuntimeError as error:errors.append(str(error))
        try:self.set_sync(snapshot['sync'])
        except RuntimeError as error:errors.append(str(error))
        if errors:raise RuntimeError('Lighting restoration incomplete: '+'; '.join(errors))


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--replay',action='store_true')
    parser.add_argument('--offline',action='store_true')
    parser.add_argument('--restore-sync',choices=['0','1'],help='Restore only the captured MBSync flag, with readback')
    args=parser.parse_args()
    sequences=captured_sequences()
    if c.sizeof(c.c_void_p)!=8 or c.sizeof(I2CInfo)!=64:raise RuntimeError('Unexpected native NVAPI structure layout')
    print('Exact vendor sequences validated; native I2C layout is 64 bytes.',flush=True)
    if args.offline:return
    gpu=GPU()
    snapshot={'rgb':gpu.read(2,3),'mode':gpu.read(5),'mode2':gpu.read(6),'sync':gpu.read(7)}
    print('Current lighting:',{k:v.hex() for k,v in snapshot.items()},flush=True)
    if args.restore_sync is not None:
        gpu.set_sync(bytes([int(args.restore_sync)]))
        return
    if not args.replay:
        print('Read-only inspection completed. No writes sent.',flush=True)
        return
    try:
        gpu.set_sync(b'\x00')
        for name,label in [('RED','RED_AGAIN'),('GREEN','GREEN'),('BLUE','BLUE'),('RED','RED_AGAIN')]:
            print(f'{name}: watch GPU lighting for five seconds.',flush=True)
            for register,data in sequences[label]:gpu.write(register,data)
            time.sleep(5)
    finally:
        print('Restoring the GPU lighting settings read before this test.',flush=True)
        gpu.restore(snapshot)
    print('Replay completed and previous settings restored. Confirm physical colours with Codex.',flush=True)


if __name__=='__main__':main()
