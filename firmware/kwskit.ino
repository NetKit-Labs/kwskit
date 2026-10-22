#include "kws_pipeline.h"

void setup() {
    if (!KwsPipeline::begin())
        while (true) {
            KwsPipeline::poll();
        }
}

void loop() {
    KwsPipeline::poll();
}
