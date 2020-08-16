#include <jni.h>
#include <string>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/aruco.hpp>
#include <opencv2/aruco/charuco.hpp>
#include <opencv2/calib3d.hpp>
#include <android/log.h>
#include <cmath>

extern "C" JNIEXPORT jstring JNICALL
Java_parsleyj_arucoslam_MainActivity_ndkLibReadyCheck(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "JNILibOk";
    return env->NewStringUTF(hello.c_str());
}


cv::Mat *castToMatPtr(jlong addr) {
    return (cv::Mat *) addr;
}

template<typename T>
cv::Ptr<T> jlongToCvPtr(jlong addr) {
    return cv::Ptr<T>((T *) addr);
}

template<typename T>
jlong cvPtrToJlong(cv::Ptr<T> ptr) {
    return (jlong) ptr.get();
}


extern "C"
JNIEXPORT jlong JNICALL
Java_parsleyj_arucoslam_MainActivity_genDictionary(JNIEnv *env, jobject thiz) {
    return cvPtrToJlong(cv::aruco::getPredefinedDictionary(cv::aruco::DICT_6X6_250));
}

extern "C"
JNIEXPORT jlong JNICALL
Java_parsleyj_arucoslam_MainActivity_genCalibrationBoard(JNIEnv *env, jobject thiz, jint markersX,
                                                         jint markersY, jfloat markerLength,
                                                         jfloat markerSeparation,
                                                         jlong ditionaryAddr) {

    cv::Ptr<cv::aruco::Dictionary> dictionary = jlongToCvPtr<cv::aruco::Dictionary>(ditionaryAddr);


    cv::Ptr<cv::aruco::GridBoard> gridboard =
            cv::aruco::GridBoard::create(
                    markersX,
                    markersY,
                    markerLength,
                    markerSeparation,
                    dictionary
            ); // create aruco board
    return cvPtrToJlong(gridboard.staticCast<cv::aruco::Board>());
}





extern "C"
JNIEXPORT void JNICALL
Java_parsleyj_arucoslam_NativeMethods_processCameraFrame(JNIEnv *env, jclass clazz,
                                                         jlong cameraMatrixAddr,
                                                         jlong distCoeffsAddr,
                                                         jlong input_mat_addr,
                                                         jlong result_mat_addr,
                                                         jdoubleArray outrvec,
                                                         jdoubleArray outtvec
) {
    auto dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_6X6_250);
    cv::Mat inputMat = *castToMatPtr(input_mat_addr);
    cv::Mat resultMat = *castToMatPtr(result_mat_addr);
    cv::Mat cameraMatrix = *castToMatPtr(cameraMatrixAddr);
    cv::Mat distCoeffs = *castToMatPtr(distCoeffsAddr);

    std::ostringstream a;
    a << cameraMatrix << std::endl;
    __android_log_print(ANDROID_LOG_DEBUG, "native-lib:processCameraFrame",
                        "cameraMatrix == %s", a.str().c_str());

    std::vector<cv::Mat> channels(3);
    cv::split(inputMat, channels);

    cv::merge(channels, resultMat);

    cv::cvtColor(inputMat, inputMat, CV_RGBA2RGB);
    std::vector<int> ids;
    std::vector<std::vector<cv::Point2f>> corners;

    __android_log_print(ANDROID_LOG_DEBUG, "native-lib:processCameraFrame",
                        "inputMat.channels() == %d", inputMat.channels());
    cv::aruco::detectMarkers(inputMat, dictionary, corners, ids,
                             cv::aruco::DetectorParameters::create(), cv::noArray(), cameraMatrix,
                             distCoeffs);

    __android_log_print(ANDROID_LOG_DEBUG, "native-lib:processCameraFrame",
                        "detectedMarkers == %d", ids.size());

    cv::Mat tmpMat;
//     if at least one marker detected
    cv::cvtColor(resultMat, tmpMat, CV_RGBA2RGB);

    if (!ids.empty()) {
        __android_log_print(ANDROID_LOG_DEBUG, "native-lib:processCameraFrame",
                            "DRAWING DETECTORS");
        cv::aruco::drawDetectedMarkers(tmpMat, corners, ids);
    }

//
    std::vector<cv::Vec3d> rvecs, tvecs;
    cv::aruco::estimatePoseSingleMarkers(
            corners,
            0.078,
            cameraMatrix,
            distCoeffs,
            rvecs,
            tvecs
            );
    for (int i = 0; i < rvecs.size(); i++) {
        auto rvec = rvecs[i];
        auto tvec = tvecs[i];

        __android_log_print(ANDROID_LOG_DEBUG, "native-lib:processCameraFrame",
                            "RVEC = [%f, %f, %f]", rvec[0], rvec[1], rvec[2]);
        __android_log_print(ANDROID_LOG_DEBUG, "native-lib:processCameraFrame",
                            "TVEC = [%f, %f, %f]", tvec[0], tvec[1], tvec[2]);
        if (i == 0) {
            for (int vi = 0; vi < 3; vi++) {
                env->SetDoubleArrayRegion(outrvec, vi, 1, &rvec[vi]);
                env->SetDoubleArrayRegion(outtvec, vi, 1, &tvec[vi]);
            }
        }
        cv::aruco::drawAxis(tmpMat, cameraMatrix, distCoeffs, rvec, tvec, 0.05);
    }

    cv::cvtColor(tmpMat, resultMat, CV_RGB2RGBA);

    __android_log_print(ANDROID_LOG_VERBOSE, "native-lib:processCameraFrame",
                        "resultMat.size() == %d x %d", resultMat.size().width,
                        resultMat.size().height);
}


extern "C"
JNIEXPORT int JNICALL
Java_parsleyj_arucoslam_NativeMethods_detectCalibrationCorners(
        JNIEnv *env,
        jclass clazz,
        jlong input_mat_addr,
        jobjectArray cornersPoints,
        jintArray idsVect,
        jintArray size,
        jint maxMarkers
) {
    __android_log_print(ANDROID_LOG_VERBOSE, "native-lib.cpp",
                        "Invoked detect calibration corners");

    auto image = *castToMatPtr(input_mat_addr);
    auto dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_6X6_250);

    __android_log_print(ANDROID_LOG_VERBOSE, "native-lib.cpp",
                        "Input casted");

    std::vector<int> ids;
    std::vector<std::vector<cv::Point2f>> corners;

    cv::aruco::detectMarkers(image, dictionary, corners, ids);

    __android_log_print(ANDROID_LOG_VERBOSE, "native-lib.cpp",
                        "Corners detected");

    if (ids.empty()) {
        __android_log_print(ANDROID_LOG_VERBOSE, "native-lib.cpp",
                            "Empty ids!");
        return 0;
    }


    __android_log_print(ANDROID_LOG_VERBOSE, "native-lib.cpp",
                        "ids.size == %d", ids.size());

    for (int csi = 0; csi < fmin(corners.size(), maxMarkers); csi++) {
        std::vector<cv::Point2f> &cornerSet = corners[csi];
        auto fourCorners = env->NewFloatArray(8);

        for (int i = 0; i < 8; i += 2) {
            cv::Point2f &point = cornerSet[i];
            env->SetFloatArrayRegion(fourCorners, i, 1, &point.x);
            env->SetFloatArrayRegion(fourCorners, i + 1, 1, &point.y);
        }

        env->SetObjectArrayElement(cornersPoints, csi, fourCorners);
    }

    for (int i = 0; i < fmin(ids.size(), maxMarkers); i++) {
        env->SetIntArrayRegion(idsVect, i, 1, &ids[i]);
    }

    __android_log_print(ANDROID_LOG_VERBOSE, "native-lib.cpp",
                        "Populated arrays");

    env->SetIntArrayRegion(size, 0, 1, &image.rows);
    env->SetIntArrayRegion(size, 1, 1, &image.cols);

    __android_log_print(ANDROID_LOG_VERBOSE, "native-lib.cpp",
                        "Populated size");
    return ids.size();
}



extern "C"
JNIEXPORT jdouble JNICALL
Java_parsleyj_arucoslam_NativeMethods_calibrate(
        JNIEnv *env,
        jclass clazz,
        jobjectArray collected_corners,
        jobjectArray collectedIDs,
        jint size_rows,
        jint size_cols,
        jlong camMatrixAddr,
        jlong distCoeffsAddr
) {

    auto dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_6X6_250);
    cv::Ptr<cv::aruco::GridBoard> gridboard = cv::aruco::GridBoard::create(
            //TODO getParamsForOutside
//            markersX,
//            markersY,
//            markerLength,
//            markerSeparation,
            8, 5, 500, 100,
            dictionary
    ); // create aruco board
    auto board = gridboard.staticCast<cv::aruco::Board>();

    std::vector<std::vector<std::vector<cv::Point2f>>> allCorners;
    std::vector<std::vector<int>> allIds;

    jboolean isNotCopy = false;
    for (int ci = 0; ci < env->GetArrayLength(collected_corners); ci++) {
        auto frameCorners = jobjectArray(env->GetObjectArrayElement(
                collected_corners, ci));
        std::vector<std::vector<cv::Point2f>> frameCornersExtracted;
        for (int ci2 = 0; ci2 < env->GetArrayLength(frameCorners); ci2++) {

            float *markerCorners = env->GetFloatArrayElements(
                    jfloatArray(env->GetObjectArrayElement(
                            frameCorners, ci2
                    )),
                    &isNotCopy);
            std::vector<cv::Point2f> markerCornersExtracted;
            for (int ci3 = 0; ci3 < 8; ci3 += 2) {
                markerCornersExtracted.emplace_back(markerCorners[ci3], markerCorners[ci3 + 1]);
            }
            frameCornersExtracted.push_back(markerCornersExtracted);
        }

        allCorners.push_back(frameCornersExtracted);
    }

    for (int i = 0; i < env->GetArrayLength(collectedIDs); i++) {
        auto intArray = jintArray(env->GetObjectArrayElement(
                collectedIDs, i
        ));
        int *frameIDs = env->GetIntArrayElements(
                intArray,
                &isNotCopy);
        jsize length = env->GetArrayLength(intArray);
        std::vector<int> frameIDsExtracted(length);
        for (int i2 = 0; i2 < length; i2++) {
            frameIDsExtracted.push_back(frameIDs[i2]);
        }
        allIds.push_back(frameIDsExtracted);
    }

    cv::Size imgSize(size_cols, size_rows);
    if (allIds.empty()) {
        __android_log_print(ANDROID_LOG_ERROR, "native-lib.cpp",
                            "Not enough captures for calibration");
        return 0.0;
    }
    std::vector<cv::Mat> rvecs, tvecs;
    double repError;
    // prepare data for calibration
    std::vector<std::vector<cv::Point2f>> allCornersConcatenated;
    std::vector<int> allIdsConcatenated;
    std::vector<int> markerCounterPerFrame;
    markerCounterPerFrame.reserve(allCorners.size());
    for (unsigned int i = 0; i < allCorners.size(); i++) {
        markerCounterPerFrame.push_back((int) allCorners[i].size());
        for (unsigned int j = 0; j < allCorners[i].size(); j++) {
            allCornersConcatenated.push_back(allCorners[i][j]);
            allIdsConcatenated.push_back(allIds[i][j]);
        }
    }

    cv::Mat cameraMatrix = *castToMatPtr(camMatrixAddr);
    cv::Mat distCoeffs = *castToMatPtr(distCoeffsAddr);
    // calibrate camera
    repError = cv::aruco::calibrateCameraAruco(
            allCornersConcatenated,
            allIdsConcatenated,
            markerCounterPerFrame,
            board,
            imgSize,
            cameraMatrix,
            distCoeffs,
            rvecs,
            tvecs,
            0);


    return repError;

}
extern "C"
JNIEXPORT jdouble JNICALL
Java_parsleyj_arucoslam_NativeMethods_calibrateChArUco(
        JNIEnv *env,
        jclass clazz,
        jobjectArray collected_corners,
        jobjectArray collectedIDs,
        jlongArray collectedFramesAddresses,
        jint size_rows,
        jint size_cols,
        jlong camMatrixAddr,
        jlong distCoeffsAddr
) {
    auto dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_6X6_250);


    cv::Ptr<cv::aruco::CharucoBoard> charucoboard =
            cv::aruco::CharucoBoard::create(
                    8,
                    5,
                    300,
                    200,
                    dictionary);
    cv::Ptr<cv::aruco::Board> board = charucoboard.staticCast<cv::aruco::Board>();

    std::vector<std::vector<std::vector<cv::Point2f>>> allCorners;
    std::vector<std::vector<int>> allIds;
    std::vector<cv::Mat> allFrames;

    jboolean isNotCopy = false;
    for (int ci = 0; ci < env->GetArrayLength(collected_corners); ci++) {
        auto frameCorners = jobjectArray(env->GetObjectArrayElement(
                collected_corners, ci));
        std::vector<std::vector<cv::Point2f>> frameCornersExtracted;
        for (int ci2 = 0; ci2 < env->GetArrayLength(frameCorners); ci2++) {

            float *markerCorners = env->GetFloatArrayElements(
                    jfloatArray(env->GetObjectArrayElement(
                            frameCorners, ci2
                    )),
                    &isNotCopy);
            std::vector<cv::Point2f> markerCornersExtracted;
            for (int ci3 = 0; ci3 < 8; ci3 += 2) {
                markerCornersExtracted.emplace_back(markerCorners[ci3], markerCorners[ci3 + 1]);
            }
            frameCornersExtracted.push_back(markerCornersExtracted);
        }

        allCorners.push_back(frameCornersExtracted);
    }

    for (int i = 0; i < env->GetArrayLength(collectedIDs); i++) {
        auto intArray = jintArray(env->GetObjectArrayElement(
                collectedIDs, i
        ));
        int *frameIDs = env->GetIntArrayElements(
                intArray,
                &isNotCopy);
        jsize length = env->GetArrayLength(intArray);
        std::vector<int> frameIDsExtracted(length);
        for (int i2 = 0; i2 < length; i2++) {
            frameIDsExtracted.push_back(frameIDs[i2]);
        }
        allIds.push_back(frameIDsExtracted);
    }


    jsize nFrames = env->GetArrayLength(collectedFramesAddresses);
    jlong *framesAddresses = env->GetLongArrayElements(
            collectedFramesAddresses,
            &isNotCopy
    );
    for (int i = 0; i < nFrames; i++) {
        allFrames.push_back(*castToMatPtr(framesAddresses[i]));
    }

    cv::Size imgSize(size_cols, size_rows);
    if (allIds.empty()) {
        __android_log_print(ANDROID_LOG_ERROR, "native-lib.cpp",
                            "Not enough captures for calibration");
        return 0.0;
    }


    std::vector<cv::Mat> allCharucoCorners;
    std::vector<cv::Mat> allCharucoIds;
    std::vector<cv::Mat> filteredImages;
    allCharucoCorners.reserve(nFrames);
    allCharucoIds.reserve(nFrames);

    cv::Mat cameraMatrix = *castToMatPtr(camMatrixAddr);
    cv::Mat distCoeffs = *castToMatPtr(distCoeffsAddr);



    // prepare data for aruco calibration
    std::vector<std::vector<cv::Point2f>> allCornersConcatenated;
    std::vector<int> allIdsConcatenated;
    std::vector<int> markerCounterPerFrame;
    markerCounterPerFrame.reserve(allCorners.size());
    for (unsigned int i = 0; i < allCorners.size(); i++) {
        markerCounterPerFrame.push_back((int) allCorners[i].size());
        for (unsigned int j = 0; j < allCorners[i].size(); j++) {
            allCornersConcatenated.push_back(allCorners[i][j]);
            allIdsConcatenated.push_back(allIds[i][j]);
        }
    }

    double arucoRepError;
    // calibrate camera
    arucoRepError = cv::aruco::calibrateCameraAruco(
            allCornersConcatenated,
            allIdsConcatenated,
            markerCounterPerFrame,
            board,
            imgSize,
            cameraMatrix,
            distCoeffs,
            cv::noArray(),
            cv::noArray(),
            0);

    __android_log_print(ANDROID_LOG_ERROR, "native-lib.cpp",
                        "aruco rep error = %f", arucoRepError);

    for (int i = 0; i < nFrames; i++) {
        // interpolate using camera parameters
        cv::Mat currentCharucoCorners, currentCharucoIds;
        cv::aruco::interpolateCornersCharuco(allCorners[i], allIds[i], allFrames[i], charucoboard,
                                             currentCharucoCorners, currentCharucoIds, cameraMatrix,
                                             distCoeffs);

        allCharucoCorners.push_back(currentCharucoCorners);
        allCharucoIds.push_back(currentCharucoIds);
        filteredImages.push_back(allFrames[i]);
    }

    if (allCharucoCorners.size() < 4) {
        __android_log_print(ANDROID_LOG_ERROR, "native-lib.cpp",
                            "Not enough corners for calibration");
        return 0;
    }

    double repError;
    std::vector<cv::Mat> rvecs, tvecs;
    // calibrate camera using charuco
    repError = cv::aruco::calibrateCameraCharuco(
            allCharucoCorners,
            allCharucoIds,
            charucoboard,
            imgSize,
            cameraMatrix,
            distCoeffs,
            rvecs,
            tvecs,
            0);

    __android_log_print(ANDROID_LOG_ERROR, "native-lib.cpp",
                        "rep error = %f", repError);

    return repError;
}