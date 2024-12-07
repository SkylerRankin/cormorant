#include <iostream>
#include <vector>
#include "glCommon.h"

void logGLError(const char* prefix) {
	std::vector<GLenum> errors;
	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR) {
		errors.push_back(err);
	}

	if (errors.size() > 0) {
		std::cout << prefix << ": ";
		for (auto e : errors) {
			std::cout << e << ", ";
		}
		std::cout << std::endl;
	} else {
		std::cout << prefix << ": no errors" << std::endl;
	}
}