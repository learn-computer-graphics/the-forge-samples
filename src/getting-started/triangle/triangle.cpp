#include "triangle.h"

#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <Renderer/IShaderReflection.h>
#include <Renderer/IResourceLoader.h>
#include <OS/Interfaces/ILog.h>
#include <OS/Interfaces/IInput.h>

//The-forge memory allocator
extern bool MemAllocInit(const char *name);
extern void MemAllocExit();

Triangle::Triangle() {}

Triangle::~Triangle()
{

	if (mRenderer != NULL)
	{
		waitQueueIdle(mGraphicsQueue);
		mAppUI.Unload();
		mAppUI.Exit();

		//resources
		removeResource(mTexture);
		removeResource(mVertexBuffer);
		removeResource(mIndexBuffer);
		//forge objects
		removeShader(mRenderer, mShader);
		removeRootSignature(mRenderer, mRootSignature);
		removeDescriptorSet(mRenderer, mDescriptorSet);
		removePipeline(mRenderer, mGraphicsPipeline);
		removeSampler(mRenderer, mSampler);
		removeSwapChain(mRenderer, mSwapChain);
		removeRenderTarget(mRenderer, mDepthBuffer);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(mRenderer, mRenderCompleteFences[i]);
			removeSemaphore(mRenderer, mRenderCompleteSemaphores[i]);
			removeCmd(mRenderer, mCmds[i]);
			removeCmdPool(mRenderer, mCmdPools[i]);
		}

		removeSemaphore(mRenderer, mImageAcquiredSemaphore);
		removeQueue(mRenderer, mGraphicsQueue);

		exitResourceLoaderInterface(mRenderer);
		removeRenderer(mRenderer);
	}

	Log::Exit();
	exitFileSystem();
	MemAllocExit();
}

bool Triangle::init(GLFWwindow *pWindow)
{
	//store the window pointer
	mWindow = pWindow;

	//init memory allocator
	if (!MemAllocInit(getName()))
	{
		printf("Failed to init memory allocator\n");
		return false;
	}

	FileSystemInitDesc fsDesc = {};
	fsDesc.pAppName = getName();

	//init file system
	if (!initFileSystem(&fsDesc))
	{
		printf("Failed to init file system\n");
		return false;
	}

	//set root directory for the log, must set this before we initialize the log
	fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_LOG, "");

	//init the log
	Log::Init(getName());

	//work out which api we are using
	RendererApi api;
#if defined(VULKAN)
	api = RENDERER_API_VULKAN;
#elif defined(DIRECT3D12)
	api = RENDERER_API_D3D12;
#else
#error Trying to use a renderer API not supported by this demo
#endif

	//set directories for the selected api
	switch (api)
	{
	case RENDERER_API_D3D12:
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES, "shaders/d3d12/");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "shaders/d3d12/binary/");
		break;
	case RENDERER_API_VULKAN:
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES, "shaders/vulkan/");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "shaders/vulkan/binary/");
		break;
	default:
		LOGF(LogLevel::eERROR, "No support for this API");
		return false;
	}

	//set texture dir
	fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "textures/");
	//set font dir
	fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "fonts/");
	//set GPUCfg dir
	fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG, "gpucfg/");

	//get framebuffer size, it may be different from window size
	glfwGetFramebufferSize(pWindow, &mFbWidth, &mFbHeight);

	//init renderer interface
	RendererDesc rendererDesc = {};
	initRenderer(getName(), &rendererDesc, &mRenderer);
	if (mRenderer == NULL)
		return false;

	//init resource loader interface
	initResourceLoaderInterface(mRenderer);

	//create graphics queue
	QueueDesc queueDesc = {};
	queueDesc.mType = QUEUE_TYPE_GRAPHICS;
	queueDesc.mFlag = QUEUE_FLAG_NONE; //use QUEUE_FLAG_INIT_MICROPROFILE to enable profiling;
	addQueue(mRenderer, &queueDesc, &mGraphicsQueue);

	for (uint32_t i = 0; i < gImageCount; ++i)
	{
		//sync objects
		addFence(mRenderer, &mRenderCompleteFences[i]);
		addSemaphore(mRenderer, &mRenderCompleteSemaphores[i]);

		//command pool for the graphics queue
		CmdPoolDesc cmdPoolDesc = {};
		cmdPoolDesc.pQueue = mGraphicsQueue;
		addCmdPool(mRenderer, &cmdPoolDesc, &mCmdPools[i]);

		//command buffer
		CmdDesc cmdDesc = {};
		cmdDesc.pPool = mCmdPools[i];
		addCmd(mRenderer, &cmdDesc, &mCmds[i]);
	}
	addSemaphore(mRenderer, &mImageAcquiredSemaphore);

	//UI - create before swapchain as createSwapchainResources calls into mAppUI
	if (!mAppUI.Init(mRenderer))
		return false;

	mAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf");

	//Load action for the render and depth target
	mLoadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
	mLoadActions.mClearColorValues[0].r = 0.2f;
	mLoadActions.mClearColorValues[0].g = 0.2f;
	mLoadActions.mClearColorValues[0].b = 0.2f;
	mLoadActions.mClearColorValues[0].a = 0.0f;
	mLoadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
	mLoadActions.mClearDepth.depth = 1.0f;
	mLoadActions.mClearDepth.stencil = 0;

	//create swapchain and depth buffer
	if (!createSwapchainResources())
		return false;

	//vertex buffer
	{
		const eastl::vector<Vertex> vertices =
			{
				Vertex(-1.0f, -1.0f, -1.0f, 0.0f, 1.0f),
				Vertex(-1.0f, 1.0f, -1.0f, 0.0f, 0.0f),
				Vertex(1.0f, 1.0f, -1.0f, 1.0f, 0.0f),
				Vertex(1.0f, -1.0f, -1.0f, 1.0f, 1.0f),

				Vertex(-1.0f, -1.0f, 1.0f, 1.0f, 1.0f),
				Vertex(1.0f, -1.0f, 1.0f, 0.0f, 1.0f),
				Vertex(1.0f, 1.0f, 1.0f, 0.0f, 0.0f),
				Vertex(-1.0f, 1.0f, 1.0f, 1.0f, 0.0f),

				Vertex(-1.0f, 1.0f, -1.0f, 0.0f, 1.0f),
				Vertex(-1.0f, 1.0f, 1.0f, 0.0f, 0.0f),
				Vertex(1.0f, 1.0f, 1.0f, 1.0f, 0.0f),
				Vertex(1.0f, 1.0f, -1.0f, 1.0f, 1.0f),

				Vertex(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f),
				Vertex(1.0f, -1.0f, -1.0f, 0.0f, 1.0f),
				Vertex(1.0f, -1.0f, 1.0f, 0.0f, 0.0f),
				Vertex(-1.0f, -1.0f, 1.0f, 1.0f, 0.0f),

				Vertex(-1.0f, -1.0f, 1.0f, 0.0f, 1.0f),
				Vertex(-1.0f, 1.0f, 1.0f, 0.0f, 0.0f),
				Vertex(-1.0f, 1.0f, -1.0f, 1.0f, 0.0f),
				Vertex(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f),

				Vertex(1.0f, -1.0f, -1.0f, 0.0f, 1.0f),
				Vertex(1.0f, 1.0f, -1.0f, 0.0f, 0.0f),
				Vertex(1.0f, 1.0f, 1.0f, 1.0f, 0.0f),
				Vertex(1.0f, -1.0f, 1.0f, 1.0f, 1.0f),
			};

		BufferLoadDesc desc = {};
		desc.ppBuffer = &mVertexBuffer;
		desc.pData = vertices.data();
		desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		desc.mDesc.mSize = vertices.size() * sizeof(Vertex);
		desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;

		addResource(&desc, NULL);
	}

	//index buffer
	{
		const eastl::vector<uint16_t> indices =
			{
				0, 1, 2,
				0, 2, 3,
				4, 5, 6,
				4, 6, 7,
				8, 9, 10,
				8, 10, 11,
				12, 13, 14,
				12, 14, 15,
				16, 17, 18,
				16, 18, 19,
				20, 21, 22,
				20, 22, 23};

		mIndexCount = (uint16_t)indices.size();

		BufferLoadDesc desc = {};
		desc.ppBuffer = &mIndexBuffer;
		desc.pData = indices.data();
		desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
		desc.mDesc.mSize = mIndexCount * sizeof(uint16_t);
		desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;

		addResource(&desc, NULL);
	}

	//texture
	{
		TextureLoadDesc desc = {};
		desc.ppTexture = &mTexture;
		//the resource loader will add the filename extension, in this case it will be dds
		desc.pFileName = "the-forge";

		addResource(&desc, NULL);
	}

	//sampler
	{
		//trilinear
		SamplerDesc desc = {FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_LINEAR,
							ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE};

		addSampler(mRenderer, &desc, &mSampler);
	}

	//shader
	{
		ShaderLoadDesc desc = {};
		desc.mStages[0] = {"demo.vert", NULL, 0};
		desc.mStages[1] = {"demo.frag", NULL, 0};
		desc.mTarget = (ShaderTarget)mRenderer->mShaderTarget;

		addShader(mRenderer, &desc, &mShader);
	}

	//root signature
	{
		const char *pStaticSamplers[] = {"samplerState0"};
		RootSignatureDesc desc = {};
		desc.mStaticSamplerCount = 1;
		desc.ppStaticSamplerNames = pStaticSamplers;
		desc.ppStaticSamplers = &mSampler;
		desc.mShaderCount = 1;
		desc.ppShaders = &mShader;

		addRootSignature(mRenderer, &desc, &mRootSignature);
	}

	//wait for our resource loads to complete, we need the texture for the descriptor set
	waitForAllResourceLoads();

	//descriptor set
	{
		//create the descriptorset
		DescriptorSetDesc desc = {mRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1};
		addDescriptorSet(mRenderer, &desc, &mDescriptorSet);

		//now update the data
		DescriptorData params[1] = {};
		params[0].pName = "texture0";
		params[0].ppTextures = &mTexture;
		updateDescriptorSet(mRenderer, 0, mDescriptorSet, 1, params);
	}

	//pipeline state object
	{
		//vertex layout
		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 2;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;
		vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
		vertexLayout.mAttribs[1].mBinding = 0;
		vertexLayout.mAttribs[1].mLocation = 1;
		vertexLayout.mAttribs[1].mOffset = 12;

		//rasterizer
		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_BACK;

		//depth state
		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;

		//pipeline
		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc &pipelineSettings = desc.mGraphicsDesc;
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = &depthStateDesc;
		pipelineSettings.pColorFormats = &mSwapChain->ppRenderTargets[0]->mFormat;
		pipelineSettings.mSampleCount = mSwapChain->ppRenderTargets[0]->mSampleCount;
		pipelineSettings.mSampleQuality = mSwapChain->ppRenderTargets[0]->mSampleQuality;
		pipelineSettings.mDepthStencilFormat = mDepthBuffer->mFormat;
		pipelineSettings.pRootSignature = mRootSignature;
		pipelineSettings.pShaderProgram = mShader;
		pipelineSettings.pVertexLayout = &vertexLayout;
		pipelineSettings.pRasterizerState = &rasterizerStateDesc;

		addPipeline(mRenderer, &desc, &mGraphicsPipeline);
	}

	//add a gui component
	{
		GuiDesc desc = {};
		const float dpiScale = getDpiScale().x;
		desc.mStartPosition = vec2(10.0f, 10.0f) / dpiScale;
		desc.mStartSize = vec2(120.0f, 110.0f) / dpiScale;

		mGuiWindow = mAppUI.AddGuiComponent("Gui Test", &desc);
		mGuiWindow->AddWidget(CheckboxWidget("V-Sync", &mVSyncEnabled));
		mGuiWindow->AddWidget(SliderFloatWidget("Rotation Speed", &mRotationSpeed, 0.0f, 1.0f, 0.1f));
	}

	//matrices
	const float aspect = (float)mFbWidth / (float)mFbHeight;
	mProjMatrix = glm::perspective(45.0f, aspect, 0.1f, 100.00f);
	mViewMatrix = glm::lookAt(glm::vec3(0.0f, 0.0f, -5.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

	return true;
}

void Triangle::onSize(const int32_t width, const int32_t height)
{
	//check if we even need to resize
	if (width == mFbWidth && height == mFbHeight)
		return;

	//store new dimensions
	mFbWidth = width;
	mFbHeight = height;

	//wait for the graphics queue to be idle
	waitQueueIdle(mGraphicsQueue);

	//remove old swapchain and depth buffer
	removeSwapChain(mRenderer, mSwapChain);
	removeRenderTarget(mRenderer, mDepthBuffer);
	mAppUI.Unload();

	//create new swapchain and depth buffer
	createSwapchainResources();

	//recalc projection matrix
	const float aspect = (float)mFbWidth / (float)mFbHeight;
	mProjMatrix = glm::perspective(45.0f, aspect, 0.1f, 100.00f);
}

void Triangle::onMouseButton(int32_t button, int32_t action)
{
	//the-forge only wants to know about left mouse button for the gui
	if (button != GLFW_MOUSE_BUTTON_1)
		return;

	bool buttonPressed = false;
	if (action == GLFW_PRESS)
		buttonPressed = true;

	//the-forge has input bindings that are designed for game controllers, it seems to map left mouse button to BUTTON_SOUTH
	mAppUI.OnButton(InputBindings::BUTTON_SOUTH, buttonPressed, &mMousePosition);
}

bool Triangle::createSwapchainResources()
{
	WindowHandle handle;
#ifdef _WIN32
	handle.window = glfwGetWin32Window(mWindow);
#else
	handle.window = glfwGetX11Window(mWindow);
	handle.display = glfwGetX11Display();
#endif

	//create swapchain
	{
		SwapChainDesc desc = {};
		desc.mWindowHandle = handle;
		desc.mPresentQueueCount = 1;
		desc.ppPresentQueues = &mGraphicsQueue;
		desc.mWidth = mFbWidth;
		desc.mHeight = mFbHeight;
		desc.mImageCount = gImageCount;
		desc.mColorFormat = getRecommendedSwapchainFormat(true);
		desc.mEnableVsync = true;
		desc.mColorClearValue = mLoadActions.mClearColorValues[0];
		addSwapChain(mRenderer, &desc, &mSwapChain);

		if (mSwapChain == NULL)
			return false;
	}

	//create depth buffer
	{
		RenderTargetDesc desc = {};
		desc.mArraySize = 1;
		desc.mClearValue = mLoadActions.mClearDepth;
		desc.mDepth = 1;
		desc.mFormat = TinyImageFormat_D32_SFLOAT;
		desc.mHeight = mFbHeight;
		desc.mSampleCount = SAMPLE_COUNT_1;
		desc.mSampleQuality = 0;
		desc.mWidth = mFbWidth;
		desc.mFlags = TEXTURE_CREATION_FLAG_ON_TILE;
		addRenderTarget(mRenderer, &desc, &mDepthBuffer);

		if (mDepthBuffer == NULL)
			return false;
	}

	if (!mAppUI.Load(mSwapChain->ppRenderTargets))
		return false;

	return true;
}

void Triangle::onRender()
{
	//delta time
	const float deltaTime = mTimer.GetMSec(true) / 1000.0f;

	//mouse pos
	if (glfwGetWindowAttrib(mWindow, GLFW_FOCUSED))
	{
		double mouseX, mouseY;
		glfwGetCursorPos(mWindow, &mouseX, &mouseY);
		mMousePosition = {(float)mouseX, (float)mouseY};
	}
	else
	{
		mMousePosition = {-1.0f, -1.0f};
	}

	//update UI
	mAppUI.Update(deltaTime);

	//cube rotation - make it spin slowly
	mRotation += deltaTime * mRotationSpeed;
	mModelMatrix = glm::rotate(glm::mat4(1.0f), mRotation * glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	mModelMatrix = glm::rotate(mModelMatrix, mRotation * glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	//recalculate world view projection matrix
	const glm::mat4 worldViewProj = mProjMatrix * mViewMatrix * mModelMatrix;

	//aquire the next swapchain image
	uint32_t swapchainImageIndex;
	acquireNextImage(mRenderer, mSwapChain, mImageAcquiredSemaphore, NULL, &swapchainImageIndex);

	//make it easier on our fingers :)
	RenderTarget *pRenderTarget = mSwapChain->ppRenderTargets[swapchainImageIndex];
	Semaphore *pRenderCompleteSemaphore = mRenderCompleteSemaphores[mFrameIndex];
	Fence *pRenderCompleteFence = mRenderCompleteFences[mFrameIndex];

	// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
	FenceStatus fenceStatus;
	getFenceStatus(mRenderer, pRenderCompleteFence, &fenceStatus);
	if (fenceStatus == FENCE_STATUS_INCOMPLETE)
		waitForFences(mRenderer, 1, &pRenderCompleteFence);

	// Reset cmd pool for this frame
	resetCmdPool(mRenderer, mCmdPools[mFrameIndex]);

	//command buffer for this frame
	Cmd *pCmd = mCmds[mFrameIndex];
	beginCmd(pCmd);

	//transition our render target to a state that we can write to
	RenderTargetBarrier barriers[] = {
		{pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET},
	};
	cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, 1, barriers);

	//bind render and depth target and set the viewport and scissor rectangle
	mLoadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
	mLoadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
	cmdBindRenderTargets(pCmd, 1, &pRenderTarget, mDepthBuffer, &mLoadActions, NULL, NULL, -1, -1);
	cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
	cmdSetScissor(pCmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

	//bind descriptor set
	cmdBindDescriptorSet(pCmd, 0, mDescriptorSet);
	//bind pipeline state object
	cmdBindPipeline(pCmd, mGraphicsPipeline);
	//bind index buffer
	cmdBindIndexBuffer(pCmd, mIndexBuffer, INDEX_TYPE_UINT16, 0);
	//bind vert buffer
	const uint32_t stride = sizeof(Vertex);
	cmdBindVertexBuffer(pCmd, 1, &mVertexBuffer, &stride, NULL);
	//bind the push constant
	cmdBindPushConstants(pCmd, mRootSignature, "UniformBlockRootConstant", &worldViewProj);
	//draw our cube
	cmdDrawIndexed(pCmd, mIndexCount, 0, 0);

	//draw UI - we want the swapchain render target bound without the depth buffer
	mLoadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
	mLoadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;
	cmdBindRenderTargets(pCmd, 1, &pRenderTarget, NULL, &mLoadActions, NULL, NULL, -1, -1);
	mAppUI.Gui(mGuiWindow);
	mAppUI.Draw(pCmd);

	//make sure no render target is bound
	cmdBindRenderTargets(pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
	//transition render target to a present state
	barriers[0] = {pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT};
	cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, 1, barriers);

	//end the command buffer
	endCmd(pCmd);

	//submit the graphics queue
	QueueSubmitDesc submitDesc = {};
	submitDesc.mCmdCount = 1;
	submitDesc.mSignalSemaphoreCount = 1;
	submitDesc.mWaitSemaphoreCount = 1;
	submitDesc.ppCmds = &pCmd;
	submitDesc.ppSignalSemaphores = &pRenderCompleteSemaphore;
	submitDesc.ppWaitSemaphores = &mImageAcquiredSemaphore;
	submitDesc.pSignalFence = pRenderCompleteFence;
	queueSubmit(mGraphicsQueue, &submitDesc);

	//present the graphics queue
	QueuePresentDesc presentDesc = {};
	presentDesc.mIndex = swapchainImageIndex;
	presentDesc.mWaitSemaphoreCount = 1;
	presentDesc.pSwapChain = mSwapChain;
	presentDesc.ppWaitSemaphores = &pRenderCompleteSemaphore;
	presentDesc.mSubmitDone = true;
	queuePresent(mGraphicsQueue, &presentDesc);

	//check v-sync
	if (mSwapChain->mEnableVsync != mVSyncEnabled)
	{
		waitQueueIdle(mGraphicsQueue);
		toggleVSync(mRenderer, &mSwapChain);
	}

	mFrameIndex = (mFrameIndex + 1) % gImageCount;
}
