// CIS565 CUDA Raytracer: A parallel raytracer for Patrick Cozzi's CIS565: GPU Computing at the University of Pennsylvania
// Written by Yining Karl Li, Copyright (c) 2012 University of Pennsylvania
// This file includes code from:
//       Rob Farber for CUDA-GL interop, from CUDA Supercomputing For The Masses: http://www.drdobbs.com/architecture-and-design/cuda-supercomputing-for-the-masses-part/222600097
//       Varun Sampath and Patrick Cozzi for GLSL Loading, from CIS565 Spring 2012 HW5 at the University of Pennsylvania: http://cis565-spring-2012.github.com/
//       Yining Karl Li's TAKUA Render, a massively parallel pathtracing renderer: http://www.yiningkarlli.com

#include "main.h"

//-------------------------------
//-------------MAIN--------------
//-------------------------------
void printDevProp(cudaDeviceProp devProp)
{
	printf("Major revision number:         %d\n",  devProp.major);
	printf("Minor revision number:         %d\n",  devProp.minor);
	printf("Name:                          %s\n",  devProp.name);
	printf("Total global memory:           %u\n",  devProp.totalGlobalMem);
	printf("Total shared memory per block: %u\n",  devProp.sharedMemPerBlock);
	printf("Total registers per block:     %d\n",  devProp.regsPerBlock);
	printf("Warp size:                     %d\n",  devProp.warpSize);
	printf("Maximum memory pitch:          %u\n",  devProp.memPitch);
	printf("Maximum threads per block:     %d\n",  devProp.maxThreadsPerBlock);
	for (int i = 0; i < 3; ++i)
		printf("Maximum dimension %d of block:  %d\n", i, devProp.maxThreadsDim[i]);
	for (int i = 0; i < 3; ++i)
		printf("Maximum dimension %d of grid:   %d\n", i, devProp.maxGridSize[i]);
	printf("Clock rate:                    %d\n",  devProp.clockRate);
	printf("Total constant memory:         %u\n",  devProp.totalConstMem);
	printf("Texture alignment:             %u\n",  devProp.textureAlignment);
	printf("Concurrent copy and execution: %s\n",  (devProp.deviceOverlap ? "Yes" : "No"));
	printf("Unified Addressing enabled:    %s\n",  (devProp.unifiedAddressing? "Yes" : "No"));
	printf("Number of multiprocessors:     %d\n",  devProp.multiProcessorCount);
	printf("Kernel execution timeout:      %s\n",  (devProp.kernelExecTimeoutEnabled ? "Yes" : "No"));
	printf("\n:");
	return;
}
int main(int argc, char** argv){

  #ifdef __APPLE__
	  // Needed in OSX to force use of OpenGL3.2 
	  glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, 3);
	  glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 2);
	  glfwOpenWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	  glfwOpenWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  #endif
	
  // Display properties for CUDA device
  cudaDeviceProp devProp;
  cudaGetDeviceProperties(&devProp, 0);
  printDevProp(devProp);
  // Set up pathtracer stuff
  bool loadedScene = false;
  finishedRender = false;

  targetFrame = 0;
  singleFrameMode = false;

  // Load scene file
  for(int i=1; i<argc; i++){
    string header; string data;
    istringstream liness(argv[i]);
    getline(liness, header, '='); getline(liness, data, '=');
    if(strcmp(header.c_str(), "scene")==0){
      renderScene = new scene(data);
      loadedScene = true;
    }
	else if(strcmp(header.c_str(), "frame")==0){
      targetFrame = atoi(data.c_str());
      singleFrameMode = true;
    }
  }

  if(!loadedScene){
    cout << "Error: scene file needed!" << endl;
    return 0;
  }

  // Set up camera stuff from loaded pathtracer settings
  iterations = 0;
  renderCam = &renderScene->renderCam;
  width = renderCam->resolution[0];
  height = renderCam->resolution[1];

  if(targetFrame>=renderCam->frames){
    cout << "Warning: Specified target frame is out of range, defaulting to frame 0." << endl;
    targetFrame = 0;
  }

  // Launch CUDA/GL

  #ifdef __APPLE__
	init();
  #else
	init(argc, argv);
  #endif

  initCuda();

  initVAO();
  initTextures();

  GLuint passthroughProgram;
  passthroughProgram = initShader("shaders/passthroughVS.glsl", "shaders/passthroughFS.glsl");

  glUseProgram(passthroughProgram);
  glActiveTexture(GL_TEXTURE0);

  #ifdef __APPLE__
	  // send into GLFW main loop
	  while(1){
		display();
		if (glfwGetKey(GLFW_KEY_ESC) == GLFW_PRESS || !glfwGetWindowParam( GLFW_OPENED )){
				exit(0);
		}
	  }

	  glfwTerminate();
  #else
	  glutDisplayFunc(display);
	  glutKeyboardFunc(keyboard);
	  glutMouseFunc(mouseClick);
	  glutMotionFunc(mouseMotion);
	  glutMouseWheelFunc(mouseWheel);

	  glutMainLoop();
  #endif
  return 0;
}

//-------------------------------
//---------RUNTIME STUFF---------
//-------------------------------

void runCuda(){

  // Map OpenGL buffer object for writing from CUDA on a single GPU
  // No data is moved (Win & Linux). When mapped to CUDA, OpenGL should not use this buffer
  
  if(iterations < renderCam->iterations){
    uchar4 *dptr=NULL;
    iterations++;
    cudaGLMapBufferObject((void**)&dptr, pbo);
  
    //pack geom and material arrays
    geom* geoms = new geom[renderScene->objects.size()];
    material* materials = new material[renderScene->materials.size()];
    
    for(int i=0; i<renderScene->objects.size(); i++){
      geoms[i] = renderScene->objects[i];
    }
    for(int i=0; i<renderScene->materials.size(); i++){
      materials[i] = renderScene->materials[i];
    }
    
  
    // execute the kernel
    cudaRaytraceCore(dptr, renderCam, targetFrame, iterations, materials, renderScene->materials.size(), geoms, renderScene->objects.size() );
    
    // unmap buffer object
    cudaGLUnmapBufferObject(pbo);
  }else{

    if(!finishedRender){
      //output image file
      image outputImage(renderCam->resolution.x, renderCam->resolution.y);

      for(int x=0; x<renderCam->resolution.x; x++){
        for(int y=0; y<renderCam->resolution.y; y++){
          int index = x + (y * renderCam->resolution.x);
          outputImage.writePixelRGB(renderCam->resolution.x-1-x,y,renderCam->image[index]);
        }
      }
      
      gammaSettings gamma;
      gamma.applyGamma = true;
      gamma.gamma = 1.0/2.2;
      gamma.divisor = renderCam->iterations;
      outputImage.setGammaSettings(gamma);
      string filename = renderCam->imageName;
      string s;
      stringstream out;
      out << targetFrame;
      s = out.str();
      utilityCore::replaceString(filename, ".bmp", "."+s+".bmp");
      utilityCore::replaceString(filename, ".png", "."+s+".png");
      outputImage.saveImageRGB(filename);
      cout << "Saved frame " << s << " to " << filename << endl;
      finishedRender = true;
      if(singleFrameMode==true){
        cudaDeviceReset(); 
        exit(0);
      }
    }
    if(targetFrame < renderCam->frames - 1){

      //clear image buffer and move onto next frame
      targetFrame++;
      iterations = 0;
      for(int i=0; i<renderCam->resolution.x*renderCam->resolution.y; i++){
        renderCam->image[i] = glm::vec3(0,0,0);
      }
      cudaDeviceReset(); 
      finishedRender = false;
    }
  }
  
}

#ifdef __APPLE__

	void display(){
		runCuda();

		string title = "CIS565 Render | " + utilityCore::convertIntToString(iterations) + " Iterations";
		glfwSetWindowTitle(title.c_str());

		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, pbo);
		glBindTexture(GL_TEXTURE_2D, displayImage);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, 
			  GL_RGBA, GL_UNSIGNED_BYTE, NULL);

		glClear(GL_COLOR_BUFFER_BIT);   

		// VAO, shader program, and texture already bound
		glDrawElements(GL_TRIANGLES, 6,  GL_UNSIGNED_SHORT, 0);

		glfwSwapBuffers();
	}

#else

	void display(){
		
	//	if(glutGet(GLUT_ELAPSED_TIME) - timeSinceLastFrame > 1000)
	//	{			
	//		fps = frames;		
			float executionTime = glutGet(GLUT_ELAPSED_TIME) - timeSinceLastFrame;
			timeSinceLastFrame = glutGet(GLUT_ELAPSED_TIME);
	//		frames = 0;
	//	}
	//	++frames;
	    char title[100];
		sprintf(title, "GPU Path Tracer | iterations: %d | Execution Time: %0.2fms", iterations, executionTime);

		runCuda();

		glutSetWindowTitle(title);

		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, pbo);
		glBindTexture(GL_TEXTURE_2D, displayImage);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, 
			  GL_RGBA, GL_UNSIGNED_BYTE, NULL);

		glClear(GL_COLOR_BUFFER_BIT);   

		// VAO, shader program, and texture already bound
		glDrawElements(GL_TRIANGLES, 6,  GL_UNSIGNED_SHORT, 0);

		glutSwapBuffers();
		if(animFlag) {
			glutPostRedisplay(); // mark window to be redisplayed at next frame
		}
		
	}

	void keyboard(unsigned char key, int x, int y)
	{
		std::cout << key << std::endl;
		switch (key) 
		{
			case('w'):
			case('W'):
				renderCam->positions[0] += moveSensitivity*renderCam->views[0];
				iterations = 0;
				CLEAR_IMAGE
				break;
			case('a'):
			case('A'):
				renderCam->positions[0] += moveSensitivity*glm::cross(renderCam->views[0], renderCam->ups[0]);
				iterations = 0;
				CLEAR_IMAGE
				break;
			case('s'):
			case('S'):
				renderCam->positions[0] -= moveSensitivity*renderCam->views[0];
				iterations = 0;
				CLEAR_IMAGE
				break;
			case('d'):
			case('D'):
				renderCam->positions[0] -= moveSensitivity*glm::cross(renderCam->views[0], renderCam->ups[0]);
				iterations = 0;
				CLEAR_IMAGE
				break;
			case(' '):
				animFlag = !animFlag;
				if(animFlag) glutPostRedisplay();
				break;
			case(27):
				exit(1);
				break;
		}
		
	}

	void mouseClick(int button, int state, int x, int y)
	{
		if (state == GLUT_DOWN) {
			button_mask |= 0x01 << button;
		} 
		else if (state == GLUT_UP) {
			unsigned char mask_not = ~button_mask;
			mask_not |= 0x01 << button;
			button_mask = ~mask_not;
		}

		mouse_old_x = x;
		mouse_old_y = y;
	}

	void mouseMotion(int x, int y)
	{
		float dx, dy;
		dx = (float)(x - mouse_old_x);
		dy = (float)(y - mouse_old_y);
		
		if (button_mask & 0x01) 
		{// left button
			viewPhi += dx * 0.002f;
			viewTheta += dy * 0.002f;
			renderCam->views[0].x = sin(viewTheta)*sin(viewPhi);
			renderCam->views[0].y = cos(viewTheta);
			renderCam->views[0].z = sin(viewTheta)*cos(viewPhi);
			
		} 
		if (button_mask & 0x02) 
		{// middle button
			renderCam->positions[0] += 0.02f*dx*glm::cross(renderCam->views[0], renderCam->ups[0]);
			renderCam->positions[0] += 0.02f*dy*glm::cross(glm::cross(renderCam->views[0], renderCam->ups[0]), renderCam->views[0]);
		}
		if (button_mask & 0x04)
		{// right button
			renderCam->positions[0] -= 0.02f*dy*renderCam->views[0];
		}

		iterations = 0;
		CLEAR_IMAGE

		mouse_old_x = x;
		mouse_old_y = y;
	}

	void mouseWheel(int button, int dir, int x, int y)
	{
		renderCam->focalLength += dir>0 ? 1.0f : -1.0f;
		iterations = 0;
		CLEAR_IMAGE
	}


#endif




//-------------------------------
//----------SETUP STUFF----------
//-------------------------------

#ifdef __APPLE__
	void init(){

		if (glfwInit() != GL_TRUE){
			shut_down(1);      
		}

		// 16 bit color, no depth, alpha or stencil buffers, windowed
		if (glfwOpenWindow(width, height, 5, 6, 5, 0, 0, 0, GLFW_WINDOW) != GL_TRUE){
			shut_down(1);
		}

		// Set up vertex array object, texture stuff
		initVAO();
		initTextures();
	}
#else
	void init(int argc, char* argv[]){
		glutInit(&argc, argv);
		glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
		glutInitWindowSize(width, height);
		glutCreateWindow("GPU Path Tracer");

		timeSinceLastFrame = glutGet(GLUT_ELAPSED_TIME);
		// Init GLEW
		glewInit();
		GLenum err = glewInit();
		if (GLEW_OK != err)
		{
			/* Problem: glewInit failed, something is seriously wrong. */
			std::cout << "glewInit failed, aborting." << std::endl;
			exit (1);
		}

		initVAO();
		initTextures();
	}
#endif

void initPBO(GLuint* pbo){
  if (pbo) {
    // set up vertex data parameter
    int num_texels = width*height;
    int num_values = num_texels * 4;
    int size_tex_data = sizeof(GLubyte) * num_values;
    
    // Generate a buffer ID called a PBO (Pixel Buffer Object)
    glGenBuffers(1,pbo);
    // Make this the current UNPACK buffer (OpenGL is state-based)
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, *pbo);
    // Allocate data for the buffer. 4-channel 8-bit image
    glBufferData(GL_PIXEL_UNPACK_BUFFER, size_tex_data, NULL, GL_DYNAMIC_COPY);
    cudaGLRegisterBufferObject( *pbo );
  }
}

void initMeshObjects(){

	for(int i = 0; i < renderScene->objects.size(); ++i)
	{
		if(renderScene->objects[i].type == MESH)
		{
			glm::vec3* cudapositions = NULL;
			cudaMalloc((void**)&cudapositions, renderScene->objects[i].vertexCount*sizeof(glm::vec3));
			cudaMemcpy( cudapositions, renderScene->objects[i].vertexList, renderScene->objects[i].vertexCount*sizeof(glm::vec3), cudaMemcpyHostToDevice);
			renderScene->objects[i].vertexList = cudapositions;

			glm::vec3* cudafaces = NULL;
			cudaMalloc((void**)&cudafaces, renderScene->objects[i].faceCount*sizeof(glm::vec3));
			cudaMemcpy( cudafaces, renderScene->objects[i].faceList, renderScene->objects[i].faceCount*sizeof(glm::vec3), cudaMemcpyHostToDevice);
			renderScene->objects[i].faceList = cudafaces;
		}
	}


}
void initCuda(){
  // Use device with highest Gflops/s
  cudaGLSetGLDevice( compat_getMaxGflopsDeviceId() );

  initPBO(&pbo);

  initMeshObjects();
  // Clean up on program exit
  atexit(cleanupCuda);

  runCuda();
}

void initTextures(){
    glGenTextures(1,&displayImage);
    glBindTexture(GL_TEXTURE_2D, displayImage);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA,
        GL_UNSIGNED_BYTE, NULL);
}

void initVAO(void){
    GLfloat vertices[] =
    { 
        -1.0f, -1.0f, 
         1.0f, -1.0f, 
         1.0f,  1.0f, 
        -1.0f,  1.0f, 
    };

    GLfloat texcoords[] = 
    { 
        1.0f, 1.0f,
        0.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f
    };

    GLushort indices[] = { 0, 1, 3, 3, 1, 2 };

    GLuint vertexBufferObjID[3];
    glGenBuffers(3, vertexBufferObjID);
    
    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObjID[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer((GLuint)positionLocation, 2, GL_FLOAT, GL_FALSE, 0, 0); 
    glEnableVertexAttribArray(positionLocation);

    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObjID[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(texcoords), texcoords, GL_STATIC_DRAW);
    glVertexAttribPointer((GLuint)texcoordsLocation, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(texcoordsLocation);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertexBufferObjID[2]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
}

GLuint initShader(const char *vertexShaderPath, const char *fragmentShaderPath){
    GLuint program = glslUtility::createProgram(vertexShaderPath, fragmentShaderPath, attributeLocations, 2);
    GLint location;

    glUseProgram(program);
    
    if ((location = glGetUniformLocation(program, "u_image")) != -1)
    {
        glUniform1i(location, 0);
    }

    return program;
}

//-------------------------------
//---------CLEANUP STUFF---------
//-------------------------------

void cleanupCuda(){
  if(pbo) deletePBO(&pbo);
  if(displayImage) deleteTexture(&displayImage);
  deleteMeshData();

}

void deleteMeshData(){
	for(int i = 0; i < renderScene->objects.size(); ++i){
		if(renderScene->objects[i].type == MESH){
			cudaFree(renderScene->objects[i].vertexList);
			cudaFree(renderScene->objects[i].faceList);
		}
	}
}
void deletePBO(GLuint* pbo){
  if (pbo) {
    // unregister this buffer object with CUDA
    cudaGLUnregisterBufferObject(*pbo);
    
    glBindBuffer(GL_ARRAY_BUFFER, *pbo);
    glDeleteBuffers(1, pbo);
    
    *pbo = (GLuint)NULL;
  }
}

void deleteTexture(GLuint* tex){
    glDeleteTextures(1, tex);
    *tex = (GLuint)NULL;
}
 
void shut_down(int return_code){
  #ifdef __APPLE__
	glfwTerminate();
  #endif
  exit(return_code);
}
