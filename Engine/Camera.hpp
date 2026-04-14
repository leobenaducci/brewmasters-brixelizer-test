#pragma once
#include <DirectXMath.h>
#include <algorithm>

class Camera
{
public:
    Camera(DirectX::XMFLOAT3 pos = {0,0,-5})
        : m_Position(pos) 
    {
        UpdateVectors();
    }

    void Move(float forward, float strafe, float upDown, float deltaTime) {
        using namespace DirectX;

        float speed = m_MoveSpeed * deltaTime;

        XMVECTOR pos = XMLoadFloat3(&m_Position);

        XMVECTOR forwardVec = XMVectorSet(m_Forward.x, 0, m_Forward.z, 0);
        forwardVec = XMVector3Normalize(forwardVec);

        XMVECTOR rightVec = XMLoadFloat3(&m_Right);
        XMVECTOR upVec = XMVectorSet(0,1,0,0);

        pos = XMVectorMultiplyAdd(XMVectorReplicate(forward * speed), forwardVec, pos);
        pos = XMVectorMultiplyAdd(XMVectorReplicate(strafe * speed), rightVec, pos);
        pos = XMVectorMultiplyAdd(XMVectorReplicate(upDown * speed), upVec, pos);

        XMStoreFloat3(&m_Position, pos);
    }

    void Rotate(float dx, float dy) {
        m_Yaw   += dx * m_MouseSensitivity;
        m_Pitch -= dy * m_MouseSensitivity;

        const float limit = DirectX::XM_PIDIV2 - 0.01f;
        m_Pitch = std::clamp(m_Pitch, -limit, limit);

        UpdateVectors();
    }

    DirectX::XMMATRIX GetViewMatrix() const {
        using namespace DirectX;

        return XMMatrixLookToLH(
            XMLoadFloat3(&m_Position),
            XMLoadFloat3(&m_Forward),
            XMLoadFloat3(&m_Up)
        );
    }

    DirectX::XMFLOAT3 GetPosition() const noexcept {
        return m_Position;
    }

private:
    void UpdateVectors() {
        using namespace DirectX;

        float cp = cosf(m_Pitch);
        float sp = sinf(m_Pitch);
        float cy = cosf(m_Yaw);
        float sy = sinf(m_Yaw);

        XMVECTOR forward = XMVectorSet(
            cp * sy,
            sp,
            cp * cy,
            0
        );

        forward = XMVector3Normalize(forward);

        XMVECTOR worldUp = XMVectorSet(0,1,0,0);

        XMVECTOR right = XMVector3Normalize(
            XMVector3Cross(forward, worldUp)
        );

        XMVECTOR up = XMVector3Cross(right, forward);

        XMStoreFloat3(&m_Forward, forward);
        XMStoreFloat3(&m_Right, right);
        XMStoreFloat3(&m_Up, up);
    }

private:
    DirectX::XMFLOAT3 m_Position{};

    DirectX::XMFLOAT3 m_Forward{};
    DirectX::XMFLOAT3 m_Right{};
    DirectX::XMFLOAT3 m_Up{};

    float m_Yaw   = 0.0f;
    float m_Pitch = 0.0f;

    float m_MoveSpeed = 8.0f;
    float m_MouseSensitivity = 0.002f;
};