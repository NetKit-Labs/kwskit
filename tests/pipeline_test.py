"""Exercise the real coordinator with deterministic module fakes."""
from pathlib import Path
import subprocess
import tempfile
with tempfile.TemporaryDirectory(prefix='kws-pipeline-') as directory:
    root = Path(directory)
    (root/'Arduino.h').write_text('''#pragma once
#include <stdint.h>
uint32_t micros(); uint32_t millis();
struct SerialFake { void println(const char*) {} };
extern SerialFake Serial;
''')
    (root/'test.cpp').write_text('''#include "kws_pipeline.h"
#include "audio_capture.h"
#include "feature_extractor.h"
#include "keyword_model.h"
#include "device_control.h"
#include "telemetry.h"
#include <Arduino.h>
#include <assert.h>
SerialFake Serial;
uint32_t clockUs=0; uint32_t micros(){return clockUs;} uint32_t millis(){return clockUs/1000;}
bool modelOk=true,featureOk=true,audioOk=true,predictOk=true,hop=true,due=true;
int queued=0, predictions=0,actions=0,resets=0,discards=0,pumps=0,reports=0;
bool led=false;
AudioCapture::Snapshot audio{};
PipelineMetrics observed;
namespace AudioCapture {
 bool begin(){return audioOk;}
 bool readHop(int16_t*,uint32_t& time){time=clockUs;return hop;}
 uint32_t available(){return queued;}
 Snapshot snapshot(){return audio;}
 void discardPending(){++discards;}
}
namespace Features {
 bool begin(float scale,int32_t zero){assert(scale==0.5f && zero==-3);return featureOk;}
 void reset(){++resets;}
 void consume(const int16_t*){clockUs+=2000;}
 bool inferenceDue(){return due;}
 void markInferred(){}
 const int8_t* data(){static int8_t features[NUM_FRAMES*NUM_MEL];return features;}
}
namespace KeywordModel {
 bool begin(){return modelOk;}
 float inputScale(){return 0.5f;} int32_t inputZeroPoint(){return -3;}
 bool predict(const int8_t*,float& p){++predictions;clockUs+=59000;p=1;return predictOk;}
}
namespace DeviceControl {
 void begin(){led=false;}
 void applyKeywordAction(){++actions;led=!led;}
 bool isOn(){return led;}
}
namespace Telemetry {
 void begin(){}
 void pump(){++pumps;}
 void report(const AudioCapture::Snapshot&,const PipelineMetrics& metrics,bool on){
  ++reports;observed=metrics;assert(on==led);
 }
}
void poll(){clockUs+=128000;KwsPipeline::poll();}
int main(){
 modelOk=false;assert(!KwsPipeline::begin());modelOk=true;
 featureOk=false;assert(!KwsPipeline::begin());featureOk=true;
 audioOk=false;assert(!KwsPipeline::begin());audioOk=true;
 assert(KwsPipeline::begin());
 hop=false;poll();assert(predictions==0 && pumps==1 && reports==1);
 hop=true;due=false;poll();assert(predictions==0);
 due=true;queued=HOP_SIZE;poll();assert(predictions==0);
 queued=0;poll();poll();assert(actions==0);poll();assert(actions==1 && led);
 assert(observed.confirmedKeywords==1 && observed.responseTiming.count==1);
 assert(observed.dspTiming.maximum==2000 && observed.inferenceTiming.maximum==59000);
 poll();assert(actions==1);
 predictOk=false;poll();assert(observed.inferenceErrors==1);
 predictOk=true;poll();poll();assert(actions==1);poll();assert(actions==2);
 audio.droppedSamples=1;poll();assert(discards==1 && resets==1);
 poll();assert(discards==1 && actions==2);poll();assert(actions==3);
}
''')
    subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-I'+directory,'-Ifirmware',
                    'firmware/kws_pipeline.cpp',str(root/'test.cpp'),'-o',str(root/'test')],check=True)
    subprocess.run([str(root/'test')],check=True)
