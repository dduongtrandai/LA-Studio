# vietnorm

Internal Vietnamese text normalization module for LA Studio. The module is
intentionally independent of LA Studio controllers, QML, model catalogs and
TTS backends so that it can later become a standalone CMake library.

The current implementation covers Unicode cleanup, Vietnamese number/date/time
conversion, currency, percentages, phone numbers, measurement units, basic
dictionary replacement and an opt-in transliteration path.
