"""Compile the actual device-control module with fake GPIO."""
from pathlib import Path
import subprocess
import tempfile
with tempfile.TemporaryDirectory(prefix='kws-device-') as directory:
    root = Path(directory)
    (root / 'Arduino.h').write_text('''#pragma once
#define LED_BUILTIN 13
#define HIGH 1
#define LOW 0
#define OUTPUT 1
void pinMode(int,int);
void digitalWrite(int,int);
''')
    (root / 'test.cpp').write_text('''#include "device_control.h"
#include <assert.h>
int pin=-1,level=-1,mode=-1;
void pinMode(int p,int m){pin=p;mode=m;}
void digitalWrite(int p,int v){pin=p;level=v;}
int main(){
 DeviceControl::begin();assert(pin==13 && mode==1 && level==0 && !DeviceControl::isOn());
 DeviceControl::applyKeywordAction();assert(level==1 && DeviceControl::isOn());
 DeviceControl::applyKeywordAction();assert(level==0 && !DeviceControl::isOn());
}
''')
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror',
                    '-I'+directory, '-Ifirmware', 'firmware/device_control.cpp',
                    str(root/'test.cpp'), '-o', str(root/'test')], check=True)
    subprocess.run([str(root/'test')], check=True)
