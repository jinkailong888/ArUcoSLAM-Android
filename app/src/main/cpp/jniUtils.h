//
// Created by pj on 19/08/2020.
//

#ifndef ARUCOSLAM_JNIUTILS_H
#define ARUCOSLAM_JNIUTILS_H

#include <jni.h>
#include <android/log.h>

#include <opencv2/core/core.hpp>

template<typename inputArrayT, typename inputElementT, typename outputVectorElementT>
void pushJavaArrayToStdVector(
        JNIEnv *env,
        inputArrayT in,
        const std::function<inputElementT *(JNIEnv *, inputArrayT, jboolean *)> &bufferResolver,
        std::vector<outputVectorElementT> &out,
        int inputArrayOffset = 0,
        int inputArrayCount = -1,
        int collectBy = 1,
        const std::function<void(const inputElementT *, outputVectorElementT &)> &collector =
        [](const inputElementT *i, outputVectorElementT &e) {
            e = *i;
        }) {

    if (inputArrayCount < 0) {
        inputArrayCount = env->GetArrayLength(in);
    }

    assert((inputArrayCount - inputArrayOffset) % collectBy == 0);

    int destCount = (inputArrayCount - inputArrayOffset) / collectBy;

    out.reserve(destCount);

    jboolean isCopy = false;

    inputElementT *buffer = bufferResolver(env, in, &isCopy);

    for (int j = 0; j < destCount; j++) {
        outputVectorElementT resultElement;

        collector(buffer + (inputArrayOffset + j * collectBy), resultElement);

        out.push_back(resultElement);
    }
}

void pushjDoubleArrayToVectorOfVec3ds(
        JNIEnv *env,
        jdoubleArray inArray,
        std::vector<cv::Vec3d> &outVectors,
        int inputArrayOffset = 0,
        int inputArrayElements = -1
) {

    pushJavaArrayToStdVector<jdoubleArray, jdouble, cv::Vec3d>(
            env,
            inArray,
            &JNIEnv::GetDoubleArrayElements,
            outVectors,
            inputArrayOffset * 3,
            inputArrayElements * 3,
            3,
            [&](const jdouble *buf, cv::Vec3d &outVec) {
                outVec[0] = buf[0];
                outVec[1] = buf[1];
                outVec[2] = buf[2];
            }
    );
}

#endif //ARUCOSLAM_JNIUTILS_H
