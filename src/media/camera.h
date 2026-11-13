#pragma once
#include "media/frame.h"
#include <memory>
#include <string>

namespace autoaimer {
/**
 * 类名称：Camera
 * 类作用：管理相机来源和图像帧借用；具体实现由构建配置选择。
 */
class Camera {
public:
    /**
     * 方法名称：Camera::Camera
     * 方法作用：建立尚未初始化的相机对象。
     */
    Camera();
    /**
     * 方法名称：Camera::~Camera
     * 方法作用：释放本对象持有的相机资源。
     */
    ~Camera();
    /**
     * 方法名称：Camera::Camera（复制）
     * 方法作用：禁止复制相机资源的所有权。
     */
    Camera(const Camera&) = delete;
    /**
     * 方法名称：Camera::operator=
     * 方法作用：禁止通过复制赋值共享相机资源。
     */
    Camera& operator=(const Camera&) = delete;

    /**
     * 方法名称：Camera::init
     * 方法作用：初始化所选相机来源；失败时通过 error 返回原因。
     */
    bool init(const std::string& iqDir, std::string& error);
    /**
     * 方法名称：Camera::acquire
     * 方法作用：借用下一帧并返回归还凭证；用状态区分成功、超时和错误。
     */
    FrameLease acquire(int timeoutMs);

private:
    struct State;
    std::unique_ptr<State> state;
};
} // namespace autoaimer
