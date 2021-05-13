#include "getting-started/triangle/triangle.h"
#include "sample.h"
#include <GLFW/glfw3.h>

//GLFW callbacks
void errorCallback(int, const char *description)
{
	printf("GLFW error: %s\n", description);
}

void framebufferResizeCallback(GLFWwindow *pWin, int w, int h)
{
	Sample *pSample = static_cast<Sample*>(glfwGetWindowUserPointer(pWin));
	assert(pSample);
	pSample->onSize(w, h);
}

void mouseButtonCallback(GLFWwindow *pWin, int button, int action, int)
{
	Sample *pSample = static_cast<Sample*>(glfwGetWindowUserPointer(pWin));
	assert(pSample);
	pSample->onMouseButton(button, action);
}

//Main
#ifdef _WIN32
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
#else
int main(int argc, const char **argv)
#endif
{
	//install glfw error callback first
	glfwSetErrorCallback(errorCallback);
	//init glfw
	if (!glfwInit())
		exit(EXIT_FAILURE);

	//we want glfw to not create any api with window creation
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
	GLFWwindow *pWindow = glfwCreateWindow(800, 600, "The-Forge Demo", NULL, NULL);
	if (!pWindow)
	{
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	//Demo class
	Triangle triangle;

	if (!triangle.init(pWindow))
	{
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	//set the demo class as the user pointer
	glfwSetWindowUserPointer(pWindow, &triangle);
	//framebuffer size callback
	glfwSetFramebufferSizeCallback(pWindow, framebufferResizeCallback);
	//mouse button callback
	glfwSetMouseButtonCallback(pWindow, mouseButtonCallback);

	while (!glfwWindowShouldClose(pWindow))
	{
		//poll events
		glfwPollEvents();

		//render
		triangle.onRender();
	}

	glfwDestroyWindow(pWindow);
	glfwTerminate();

	exit(EXIT_SUCCESS);
}
