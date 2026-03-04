#include "Lvs/Engine/Math/Matrix4.hpp"

#include <cmath>
#include <stdexcept>

namespace Lvs::Engine::Math {

Matrix4::Matrix4() = default;

Matrix4::Matrix4(const std::array<std::array<double, 4>, 4>& rows)
    : m_(rows) {
}

Matrix4 Matrix4::operator-() const {
    Matrix4 result;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result.m_[i][j] = m_[3 - i][3 - j];
        }
    }
    return result;
}

Matrix4 Matrix4::operator*(const Matrix4& other) const {
    Matrix4 out;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            double sum = 0.0;
            for (int k = 0; k < 4; ++k) {
                sum += m_[i][k] * other.m_[k][j];
            }
            out.m_[i][j] = sum;
        }
    }
    return out;
}

Matrix4 Matrix4::Identity() {
    Matrix4 matrix;
    for (int i = 0; i < 4; ++i) {
        matrix.m_[i][i] = 1.0;
    }
    return matrix;
}

Matrix4 Matrix4::Inverse() const {
    // Gauss-Jordan elimination on augmented [A | I].
    std::array<std::array<double, 8>, 4> aug{};
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            aug[row][col] = m_[row][col];
            aug[row][col + 4] = (row == col) ? 1.0 : 0.0;
        }
    }

    for (int col = 0; col < 4; ++col) {
        int pivot = col;
        for (int row = col + 1; row < 4; ++row) {
            if (std::abs(aug[row][col]) > std::abs(aug[pivot][col])) {
                pivot = row;
            }
        }

        if (std::abs(aug[pivot][col]) < 1e-12) {
            throw std::runtime_error("Matrix4 is singular and cannot be inverted.");
        }

        if (pivot != col) {
            std::swap(aug[pivot], aug[col]);
        }

        const double div = aug[col][col];
        for (double& value : aug[col]) {
            value /= div;
        }

        for (int row = 0; row < 4; ++row) {
            if (row == col) {
                continue;
            }

            const double factor = aug[row][col];
            for (int k = 0; k < 8; ++k) {
                aug[row][k] -= factor * aug[col][k];
            }
        }
    }

    Matrix4 inv;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            inv.m_[row][col] = aug[row][col + 4];
        }
    }
    return inv;
}

Matrix4 Matrix4::Translation(const Vector3& vector) {
    Matrix4 matrix = Identity();
    matrix.m_[0][3] = vector.x;
    matrix.m_[1][3] = vector.y;
    matrix.m_[2][3] = vector.z;
    return matrix;
}

Matrix4 Matrix4::Scale(const Vector3& vector) {
    Matrix4 matrix = Identity();
    matrix.m_[0][0] = vector.x;
    matrix.m_[1][1] = vector.y;
    matrix.m_[2][2] = vector.z;
    return matrix;
}

const std::array<std::array<double, 4>, 4>& Matrix4::Rows() const {
    return m_;
}

std::array<double, 16> Matrix4::FlattenRowMajor() const {
    std::array<double, 16> flat{};
    int index = 0;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            flat[index++] = m_[row][col];
        }
    }
    return flat;
}

std::array<double, 16> Matrix4::FlattenColumnMajor() const {
    std::array<double, 16> flat{};
    int index = 0;
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            flat[index++] = m_[row][col];
        }
    }
    return flat;
}

} // namespace Lvs::Engine::Math
