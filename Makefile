# Path to the Rack SDK. Override with `make RACK_DIR=/path/to/Rack-SDK`.
RACK_DIR ?= D:/Source/SDK/Rack-SDK

SOURCES += $(wildcard src/*.cpp)

DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)

include $(RACK_DIR)/plugin.mk
