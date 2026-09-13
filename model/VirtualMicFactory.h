#ifndef VIRTUALMICFACTORY_H
#define VIRTUALMICFACTORY_H

#include <memory>

class VirtualMic;

std::unique_ptr<VirtualMic> makeVirtualMic();

#endif // VIRTUALMICFACTORY_H
