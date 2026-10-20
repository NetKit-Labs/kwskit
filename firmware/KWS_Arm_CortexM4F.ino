#include "kws_pipeline.h"

void setup() {
    if (!KwsPipeline::begin())
        while (true) {
        }
}

void loop() {
    KwsPipeline::poll();
}
