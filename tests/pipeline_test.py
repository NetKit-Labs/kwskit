"""Exercise the real audio pipeline with deterministic module fakes."""
from pathlib import Path
import subprocess
import tempfile
with tempfile.TemporaryDirectory(prefix='kws-pipeline-') as directory:
    root = Path(directory)
    (root/'Arduino.h').write_text('''#pragma once
#include <stdint.h>
uint32_t micros(); uint32_t millis();
''')
    (root/'test.cpp').write_text('''#include "audio_pipeline.h"
#include "keyword_event.h"
#include "audio_capture.h"
#include "feature_extractor.h"
#include "keyword_model.h"
#include <Arduino.h>
#include <assert.h>
uint32_t clockUs=0; uint32_t micros(){return clockUs;} uint32_t millis(){return clockUs/1000;}
bool predictOk=true,hop=true,due=true;
int queued=0,predictions=0,resets=0,discards=0;
AudioCapture::Snapshot audio{};
namespace AudioCapture {
 bool readHop(int16_t*,uint32_t& time){time=clockUs;return hop;}
 uint32_t available(){return queued;}
 Snapshot snapshot(){return audio;}
 void discardPending(){++discards;}
}
namespace Features {
 void reset(){++resets;}
 void consume(const int16_t*){clockUs+=2000;}
 bool inferenceDue(){return due;}
 void markInferred(){}
 const int8_t* data(){static int8_t features[NUM_FRAMES*NUM_MEL];return features;}
}
namespace KeywordModel {
 bool predict(const int8_t*,float& p){++predictions;clockUs+=59000;p=1;return predictOk;}
}
int main(){
 AudioPipeline pipeline; KeywordEvent event{};
 hop=false;auto step=pipeline.process(event);assert(!step.processed && !step.hasEvent);
 hop=true;due=false;step=pipeline.process(event);assert(step.processed && !step.hasEvent);
 due=true;queued=HOP_SIZE;step=pipeline.process(event);assert(!step.hasEvent && predictions==0);
 queued=0;step=pipeline.process(event);assert(step.hasEvent && predictions==1);
 assert(event.probability==1 && event.decisionUs==clockUs && event.captureEndUs==clockUs-61000);
 assert(pipeline.statistics().dspTiming.maximum==2000);
 assert(pipeline.statistics().inferenceTiming.maximum==59000);
 predictOk=false;step=pipeline.process(event);
 assert(step.reset && !step.hasEvent && pipeline.statistics().inferenceErrors==1);
 predictOk=true;audio.droppedSamples=1;step=pipeline.process(event);
 assert(step.reset && discards==1 && resets==1);
 step=pipeline.process(event);assert(!step.reset && discards==1);
}
''')
    subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-I'+directory,'-Ifirmware',
                    'firmware/audio_pipeline.cpp',str(root/'test.cpp'),'-o',str(root/'test')],check=True)
    subprocess.run([str(root/'test')],check=True)
