export BUILD_DIR
export CXXFLAGS
export DEFINEDS 
export DEPEND_LIBS
export DEPEND_LIBS_DIR
export INCLUDES
export SOURCES

OBJS = $(addprefix $(BUILD_DIR)/,$(patsubst %.cpp,%.o,$(notdir $(SOURCES))))
OBJS = $(addprefix $(BUILD_DIR)/,$(patsubst %.cc,%.o,$(notdir $(SOURCES))))

DEPS = $(OBJS:.o=.d)

CXXFLAGS := -pthread -lpthread --std=c++11 -Wall  -rdynamic -O0 -D_GLIBCXX_USE_DEPRECATED=0

include vs.mk


$(exe):$(OBJS)
	$(CXX) $(OBJS) $(CXXFLAGS) $(DEFINEDS) $(INCLUDES) -o $@ $(DEPEND_LIBS) $(DEPEND_LIBS_DIR) 

$(lib):$(OBJS)
	$(AR) -rcs $@ $(OBJS) 

$(shared):
	$(CXX) -shared -fPIC $(CXXFLAGS) $(DEFINEDS) $(INCLUDES) $(SOURCES) -o $@

clean:
	@rm -rf $(OBJS) $(exec) $(lib) $(shared) $(BUILD_DIR)

#http://stackoverflow.com/questions/1963802/makefile-pattern-rules-and-directories
define define_obj_rules
$(BUILD_DIR)/%.o: $(1)%.cc
	@mkdir -p $$(@D)
	$(CXX) -c $(CXXFLAGS)  $(DEFINEDS) $(INCLUDES) $$< -o $$@ 
$(BUILD_DIR)/%.o: $(1)%.cpp
	@mkdir -p $$(@D)
	$(CXX) -c $(CXXFLAGS)  $(DEFINEDS) $(INCLUDES) $$< -o $$@ 
endef

$(foreach directory,$(sort $(dir $(SOURCES))),$(eval $(call define_obj_rules,$(directory))))
