#ifndef WINDOWUTILS_H
#define WINDOWUTILS_H

class QWidget;

namespace WindowUtils {



void prepareOpenGLComposition(QWidget *window);



void syncNativeMaximizeCapability(QWidget *window, bool enabled);

} 

#endif 
