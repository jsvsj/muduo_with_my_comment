#ifndef MUDUO_BASE_COPYABLE_H
#define MUDUO_BASE_COPYABLE_H

namespace muduo
{

/// A tag class emphasises the objects are copyable.
/// The empty base class optimization applies.
/// Any derived class of copyable should be a value type.

//�ջ��࣬��ʾ�����ǿ��Կ����ģ�ֵ����,����֮����Դ���������ϵ
class copyable
{

};

};

#endif  // MUDUO_BASE_COPYABLE_H
