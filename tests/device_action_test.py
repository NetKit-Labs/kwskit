# Compile the actual sketch action with fake GPIO and verify pin state and toggling.
from pathlib import Path
import subprocess
s = Path('firmware/KWS_Arm_CortexM4F.ino').read_text()
a = s.index('bool deviceOn = false;')
b = s.index('\n}', a) + 2
source = '''#include <assert.h>
#define LED_BUILTIN 13
#define HIGH 1
#define LOW 0
int pin=-1, level=-1;
void digitalWrite(int p,int v) {pin=p;level=v;}
''' + s[a:b] + '''
int main(){applyKeywordAction();assert(pin==13 && level==HIGH && deviceOn);
applyKeywordAction();assert(level==LOW && !deviceOn);}
'''
Path('/tmp/kws_device_test.cpp').write_text(source)
subprocess.run(['c++','/tmp/kws_device_test.cpp','-o','/tmp/kws_device_test'],check=True)
subprocess.run(['/tmp/kws_device_test'],check=True)
