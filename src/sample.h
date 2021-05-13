#pragma once

#include <cstdint>

//forward declare
struct GLFWwindow;

class Sample
{
public:
    Sample() {}
    virtual ~Sample() {}

    virtual bool init(GLFWwindow *pWindow) = 0;
	virtual void onSize(const int32_t width, const int32_t height) = 0;
	virtual void onMouseButton(int32_t button, int32_t action) = 0;
	virtual void onRender() = 0;
	virtual const char *getName() = 0;
};
