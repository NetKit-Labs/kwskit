"""Compile the actual RTOS coordinator with fake CMSIS startup/failure APIs."""
from pathlib import Path
import subprocess
import tempfile
with tempfile.TemporaryDirectory(prefix='kws-rtos-') as directory:
    root=Path(directory)
    (root/'Arduino.h').write_text('''#pragma once
#include <stdint.h>
uint32_t micros();uint32_t millis();
struct SerialFake {void println(const char*) {}};
extern SerialFake Serial;
''')
    (root/'cmsis_os2.h').write_text('''#pragma once
#include <stdint.h>
using osThreadId_t=void*;using osMessageQueueId_t=void*;using osMutexId_t=void*;
using osStatus_t=int;
constexpr int osOK=0,osPriorityAboveNormal=32,osPriorityNormal=24;
constexpr uint32_t osWaitForever=0xffffffffu,osFlagsError=0x80000000u,osFlagsWaitAny=0,osMutexPrioInherit=2;
struct osThreadAttr_t {const char* name;uint32_t attr_bits;void* cb_mem;uint32_t cb_size;void* stack_mem;uint32_t stack_size;int priority;};
struct osMessageQueueAttr_t {const char* name;uint32_t attr_bits;void* cb_mem;uint32_t cb_size;void* mq_mem;uint32_t mq_size;};
struct osMutexAttr_t {const char* name;uint32_t attr_bits;void* cb_mem;uint32_t cb_size;};
osMutexId_t osMutexNew(const osMutexAttr_t*);int osMutexDelete(osMutexId_t);
int osMutexAcquire(osMutexId_t,uint32_t);int osMutexRelease(osMutexId_t);
osThreadId_t osThreadNew(void(*)(void*),void*,const osThreadAttr_t*);
int osThreadTerminate(osThreadId_t);uint32_t osThreadGetStackSpace(osThreadId_t);
uint32_t osThreadFlagsSet(osThreadId_t,uint32_t);uint32_t osThreadFlagsWait(uint32_t,uint32_t,uint32_t);
uint32_t osKernelGetTickFreq();int osDelay(uint32_t);
osMessageQueueId_t osMessageQueueNew(uint32_t,uint32_t,const osMessageQueueAttr_t*);
int osMessageQueueDelete(osMessageQueueId_t);
int osMessageQueuePut(osMessageQueueId_t,const void*,uint8_t,uint32_t);
int osMessageQueueGet(osMessageQueueId_t,void*,uint8_t*,uint32_t);
''')
    (root/'rtx_os.h').write_text('''#pragma once
struct osRtxThread_t {uint64_t data[16];};
struct osRtxMessageQueue_t {uint64_t data[8];};
struct osRtxMutex_t {uint64_t data[8];};
#define osRtxMessageQueueMemSize(n,s) ((n)*((s)+16))
''')
    (root/'test.cpp').write_text('''#include "kws_pipeline.h"
#include "audio_pipeline.h"
#include "audio_capture.h"
#include "feature_extractor.h"
#include "keyword_model.h"
#include "device_control.h"
#include "telemetry.h"
#include <cmsis_os2.h>
#include <Arduino.h>
#include <assert.h>
#include <string.h>
SerialFake Serial;uint32_t micros(){return 0;}uint32_t millis(){return 0;}
int failure=0,mutexes=0,queues=0,threads=0,threadCalls=0,lastDelay=0,flagCalls=0;
void(*notifier)()=nullptr;
osThreadAttr_t attrs[2];
osMutexId_t osMutexNew(const osMutexAttr_t* a){
 assert(a->cb_mem && a->cb_size && a->attr_bits==osMutexPrioInherit);
 if(failure==1){return nullptr;}++mutexes;return (void*)1;
}
int osMutexDelete(osMutexId_t){--mutexes;return 0;}
int osMutexAcquire(osMutexId_t,uint32_t){return 0;}int osMutexRelease(osMutexId_t){return 0;}
osMessageQueueId_t osMessageQueueNew(uint32_t n,uint32_t size,const osMessageQueueAttr_t* a){
 assert(n==8 && size==sizeof(KeywordEvent));assert(a->cb_mem && a->mq_mem && a->mq_size>=n*size);
 if(failure==2){return nullptr;}++queues;return (void*)2;
}
int osMessageQueueDelete(osMessageQueueId_t){--queues;return 0;}
int osMessageQueuePut(osMessageQueueId_t,const void*,uint8_t,uint32_t wait){assert(wait==0);return 0;}
int osMessageQueueGet(osMessageQueueId_t,void*,uint8_t*,uint32_t){return -1;}
osThreadId_t osThreadNew(void(*entry)(void*),void*,const osThreadAttr_t* a){
 assert(entry && a->stack_mem && a->cb_mem && (uintptr_t(a->stack_mem)%8)==0);
 int call=threadCalls++;attrs[call%2]=*a;
 if((failure==3 && call==0)||(failure==4 && call==1))return nullptr;
 ++threads;return reinterpret_cast<void*>(uintptr_t(call+3));
}
int osThreadTerminate(osThreadId_t){--threads;return 0;}
uint32_t osThreadGetStackSpace(osThreadId_t){return 4096;}
uint32_t osThreadFlagsSet(osThreadId_t id,uint32_t f){assert(id);++flagCalls;return f;}
uint32_t osThreadFlagsWait(uint32_t f,uint32_t,uint32_t){return f;}
uint32_t osKernelGetTickFreq(){return 1000;}int osDelay(uint32_t t){lastDelay=t;return 0;}
namespace AudioCapture {
 bool begin(void(*callback)()){notifier=callback;return failure!=5;}
 void end(){notifier=nullptr;}
 Snapshot snapshot(){return {};}
}
namespace KeywordModel {
 bool begin(){return failure!=6;}float inputScale(){return 0.5f;}int32_t inputZeroPoint(){return 0;}
}
namespace Features {bool begin(float,int32_t){return failure!=7;}}
namespace DeviceControl {void begin(){}void applyKeywordAction(){}bool isOn(){return false;}}
namespace Telemetry {void begin(){}void pump(){}void report(const AudioCapture::Snapshot&,const PipelineMetrics&,bool){}}
AudioStep AudioPipeline::process(KeywordEvent&){(void)seenDrops;(void)audioHop;return {false,false,false};}
int main(){
 for(int f=1;f<=7;++f){failure=f;threadCalls=0;assert(!KwsPipeline::begin());
  assert(threads==0 && queues==0 && mutexes==0 && !notifier);}
 failure=0;threadCalls=0;assert(KwsPipeline::begin());
 assert(threads==2 && queues==1 && mutexes==1 && notifier);
 assert(attrs[0].priority>attrs[1].priority);
 assert(attrs[0].stack_size==8192 && attrs[1].stack_size==6144);
 assert(attrs[0].stack_mem!=attrs[1].stack_mem);
 assert(strcmp(attrs[0].name,"kws-audio")==0 && strcmp(attrs[1].name,"kws-control")==0);
 flagCalls=0;notifier();assert(flagCalls==1);
 KwsPipeline::poll();assert(lastDelay==1000);
}
''')
    subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-I'+directory,'-Ifirmware',
                    'firmware/kws_pipeline.cpp',str(root/'test.cpp'),'-o',str(root/'test')],check=True)
    subprocess.run([str(root/'test')],check=True)
