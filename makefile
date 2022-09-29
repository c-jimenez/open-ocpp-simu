######################################################
#    OCPP charge points simulator helper makefile    #
######################################################

# Root directory containing the top level CMakeLists.txt file
ROOT_DIR:=$(PWD)

# Generated binary directory
BIN_DIR:=$(ROOT_DIR)/bin

# Make options
#VERBOSE="VERBOSE=1"
PARALLEL_BUILD?=-j 4

# Build type can be either Debug or Release
BUILD_TYPE?=Debug

# If set to 1, compilation is done natively on the current os otherwise a dedicated docker container is used to build the project
DISABLE_DOCKER?=0


DOCKER_COMPILE_IMAGE=open-ocpp-simu-compile


# Default target
default: gcc-native

# Silent makefile
#.SILENT:

# Install prefix
ifneq ($(strip $(INSTALL_PREFIX)),)
CMAKE_INSTALL_PREFIX:=-D CMAKE_INSTALL_PREFIX=$(INSTALL_PREFIX)
CMAKE_INSTALL_PREFIX_CMD:=--prefix $(INSTALL_PREFIX)
endif


INTERACTIVE:=$(shell [ -t 0 ] && echo 1)
ifdef INTERACTIVE
DOCKER_INTERACTIVE=-ti
else
DOCKER_INTERACTIVE=-i
endif
ifeq ($(DISABLE_DOCKER),0)
DOCKER_RUN=docker run $(DOCKER_INTERACTIVE) --rm -v $(ROOT_DIR):$(ROOT_DIR):rw -w $(ROOT_DIR) -e http_proxy="${http_proxy}" -e https_proxy="${https_proxy}" -e no_proxy="${no_proxy}" $(DOCKER_COMPILE_IMAGE) docker/Entrypoint/entrypoint.sh
else
DOCKER_RUN=
endif


# Build/clean all targets
all: gcc-native clang-native
clean: clean-gcc-native clean-clang-native
	@-rm -rf $(BIN_DIR)

# Targets for gcc-native build
GCC_NATIVE_BUILD_DIR:=$(ROOT_DIR)/build_gcc_native
GCC_NATIVE_BIN_DIR:=$(BIN_DIR)/gcc_native
gcc-native: $(GCC_NATIVE_BUILD_DIR)/Makefile
	@echo "Starting gcc-native build..."
	@mkdir -p $(GCC_NATIVE_BIN_DIR)
	${DOCKER_RUN} eval "make --silent -C $(GCC_NATIVE_BUILD_DIR) $(VERBOSE) $(PARALLEL_BUILD)"
	@echo "gcc-native build done!"

clean-gcc-native:
	@-rm -rf $(GCC_NATIVE_BUILD_DIR)
	@-rm -rf $(GCC_NATIVE_BIN_DIR)
	@echo "gcc-native build cleaned!"

$(GCC_NATIVE_BUILD_DIR)/Makefile:
	@echo "Generating gcc-native makefiles..."
	@mkdir -p $(GCC_NATIVE_BUILD_DIR)
	@mkdir -p $(GCC_NATIVE_BIN_DIR)
	${DOCKER_RUN} eval "cd $(GCC_NATIVE_BUILD_DIR) && CC=gcc CXX=g++ cmake -D CMAKE_BUILD_TYPE=$(BUILD_TYPE) -D TARGET=native -D BIN_DIR=$(GCC_NATIVE_BIN_DIR) $(CMAKE_INSTALL_PREFIX) $(ROOT_DIR)"
	


# Targets for clang-native build
CLANG_NATIVE_BUILD_DIR:=$(ROOT_DIR)/build_clang_native
CLANG_NATIVE_BIN_DIR:=$(BIN_DIR)/clang_native
clang-native: $(CLANG_NATIVE_BUILD_DIR)/Makefile
	@echo "Starting clang native build..."
	@mkdir -p $(CLANG_NATIVE_BIN_DIR)
	@${DOCKER_RUN} eval "make --silent -C $(CLANG_NATIVE_BUILD_DIR) $(VERBOSE) $(PARALLEL_BUILD)"
	@echo "clang native build done!"

clean-clang-native:
	@-rm -rf $(CLANG_NATIVE_BUILD_DIR)
	@-rm -rf $(CLANG_NATIVE_BIN_DIR)
	@echo "clang native build cleaned!"

$(CLANG_NATIVE_BUILD_DIR)/Makefile:
	@echo "Generating clang-native makefiles..."
	@mkdir -p $(CLANG_NATIVE_BUILD_DIR)
	@mkdir -p $(CLANG_NATIVE_BIN_DIR)
	@${DOCKER_RUN} eval "cd $(CLANG_NATIVE_BUILD_DIR) && CC=clang CXX=clang++ cmake -D CMAKE_BUILD_TYPE=$(BUILD_TYPE) -D TARGET=native -D _CMAKE_TOOLCHAIN_PREFIX=llvm- -D BIN_DIR=$(CLANG_NATIVE_BIN_DIR) $(CMAKE_INSTALL_PREFIX) $(ROOT_DIR)"


DOCKER_BUILD=docker build --build-arg http_proxy="${http_proxy}" --build-arg https_proxy="${https_proxy}" --build-arg no_proxy="${no_proxy}" --build-arg uid=$$(id -u) --build-arg gid=$$(id -g)
DOCKER_SIMULATOR_IMAGE=open-occp-cp-simulator
docker-build-images: docker-build-simu-compile docker-build-cp-simulator

docker-build-simu-compile:
	@${DOCKER_BUILD}  -f docker/Dockerfile -t $(DOCKER_COMPILE_IMAGE)  $(ROOT_DIR)/docker

docker-build-cp-simulator:
	@${DOCKER_BUILD}  -f docker/Dockerfile_cp_simulator -t $(DOCKER_SIMULATOR_IMAGE)  $(ROOT_DIR)/bin/


run-simu:
	docker run $(DOCKER_INTERACTIVE) --rm  --network=host --name ocpp-simu $(DOCKER_SIMULATOR_IMAGE)
	
run-launcher:
	docker run $(DOCKER_INTERACTIVE) --rm  --network=host --name ocpp-launcher --entrypoint /cp_simulator/launcher $(DOCKER_SIMULATOR_IMAGE)

