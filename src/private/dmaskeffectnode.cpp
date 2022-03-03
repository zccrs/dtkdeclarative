/*
 * Copyright (C) 2020 Uniontech Technology Co., Ltd.
 *
 * Author:     xiaoyaobing <xiaoyaobing@uniontech.com>
 *
 * Maintainer: xiaoyaobing <xiaoyaobing@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dmaskeffectnode_p.h"
#include "private/qsgdefaultimagenode_p.h"

#include <QOpenGLContext>
#include <QOpenGLFunctions>

DQUICK_BEGIN_NAMESPACE

inline static bool isPowerOfTwo(int x)
{
    // Assumption: x >= 1
    return x == (x & -x);
}

class OpaqueTextureMaterialShader : public QSGMaterialShader
{
public:
    OpaqueTextureMaterialShader();

    void updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect) override;
    char const *const *attributeNames() const override;

protected:
    void initialize() override;

protected:
    int m_matrix_id;
};

class TextureMaterialShader : public OpaqueTextureMaterialShader
{
public:
    TextureMaterialShader();

    void updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect) override;
    void initialize() override;

protected:
    int m_opacity_id;
};

OpaqueTextureMaterialShader::OpaqueTextureMaterialShader()
{
#if QT_CONFIG(opengl)
    setShaderSourceFile(QOpenGLShader::Vertex, QStringLiteral(":/dtk/declarative/shaders/quickitemviewport-opaque.vert"));
    setShaderSourceFile(QOpenGLShader::Fragment, QStringLiteral(":/dtk/declarative/shaders/quickitemviewport-opaque.frag"));
#endif
}

char const *const *OpaqueTextureMaterialShader::attributeNames() const
{
    static char const *const attr[] = { "qt_VertexPosition", "qt_VertexTexCoord", nullptr };
    return attr;
}

void OpaqueTextureMaterialShader::initialize()
{
#if QT_CONFIG(opengl)
    m_matrix_id = program()->uniformLocation("qt_Matrix");
#endif
}

void OpaqueTextureMaterialShader::updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect)
{
    Q_ASSERT(oldEffect == nullptr || newEffect->type() == oldEffect->type());
    const OpaqueTextureMaterial *newMaterial = static_cast<OpaqueTextureMaterial *>(newEffect);

    // TODO：一直刷新数据浪费性能，需要优化
    QSGTexture *t = newMaterial->texture();
    if (Q_UNLIKELY(!t))
        return;
    t->setFiltering(newMaterial->filtering());
    t->setHorizontalWrapMode(newMaterial->horizontalWrapMode());
    t->setVerticalWrapMode(newMaterial->verticalWrapMode());

#if QT_CONFIG(opengl)
    auto gl = const_cast<QOpenGLContext *>(state.context())->functions();
    bool npotSupported = gl->hasOpenGLFeature(QOpenGLFunctions::NPOTTextureRepeat);
    if (!npotSupported) {
        QSize size = t->textureSize();
        const bool isNpot = !isPowerOfTwo(size.width()) || !isPowerOfTwo(size.height());
        if (isNpot) {
            t->setHorizontalWrapMode(QSGTexture::ClampToEdge);
            t->setVerticalWrapMode(QSGTexture::ClampToEdge);
        }
    }
#else
    Q_UNUSED(state)
#endif

    t->setMipmapFiltering(newMaterial->mipmapFiltering());
    t->setAnisotropyLevel(newMaterial->anisotropyLevel());

    OpaqueTextureMaterial *oldTx = static_cast<OpaqueTextureMaterial *>(oldEffect);
    if (oldTx == nullptr || oldTx->texture()->textureId() != t->textureId())
        t->bind();
    else
        t->updateBindOptions();

#if QT_CONFIG(opengl)
    auto mask = newMaterial->maskTexture();
    gl->glActiveTexture(GL_TEXTURE1);
    if (oldTx == nullptr || oldTx->maskTexture()->textureId() != mask->textureId()) {
        mask->bind();
    } else {
        mask->updateBindOptions();
    }
    gl->glActiveTexture(GL_TEXTURE0);

    program()->setUniformValue("sourceScale", newMaterial->sourceScale());
    program()->setUniformValue("mask", 1);
    program()->setUniformValue("maskScale", newMaterial->maskScale());
    program()->setUniformValue("maskOffset", newMaterial->maskOffset());

    if (state.isMatrixDirty())
        program()->setUniformValue(m_matrix_id, state.combinedMatrix());
#else
    Q_UNUSED(state)
#endif
}

TextureMaterialShader::TextureMaterialShader()
    : OpaqueTextureMaterialShader()
{
#if QT_CONFIG(opengl)
    setShaderSourceFile(QOpenGLShader::Fragment, ":/dtk/declarative/shaders/quickitemviewport.frag");
#endif
}

void TextureMaterialShader::updateState(const QSGMaterialShader::RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect)
{
    Q_ASSERT(oldEffect == nullptr || newEffect->type() == oldEffect->type());
#if QT_CONFIG(opengl)
    if (state.isOpacityDirty())
        program()->setUniformValue(m_opacity_id, state.opacity());
#endif
    OpaqueTextureMaterialShader::updateState(state, newEffect, oldEffect);
}

void TextureMaterialShader::initialize()
{
    OpaqueTextureMaterialShader::initialize();
#if QT_CONFIG(opengl)
    m_opacity_id = program()->uniformLocation("opacity");
#endif
}

MaskEffectNode::MaskEffectNode()
    : m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
    , m_texCoordMode(QSGDefaultImageNode::NoTransform)
    , m_isAtlasTexture(false)
    , m_ownsTexture(false)
{
    setGeometry(&m_geometry);
    setMaterial(&m_material);
    setOpaqueMaterial(&m_opaque_material);
    m_material.setMipmapFiltering(QSGTexture::None);
    m_opaque_material.setMipmapFiltering(QSGTexture::None);
}

MaskEffectNode::~MaskEffectNode()
{
    if (m_ownsTexture)
        delete m_material.texture();
}

void MaskEffectNode::setFiltering(QSGTexture::Filtering filtering)
{
    if (m_material.filtering() == filtering)
        return;

    m_material.setFiltering(filtering);
    m_opaque_material.setFiltering(filtering);
    markDirty(DirtyMaterial);
}

QSGTexture::Filtering MaskEffectNode::filtering() const
{
    return m_material.filtering();
}

void MaskEffectNode::setMipmapFiltering(QSGTexture::Filtering filtering)
{
    if (m_material.mipmapFiltering() == filtering)
        return;

    m_material.setMipmapFiltering(filtering);
    m_opaque_material.setMipmapFiltering(filtering);
    markDirty(DirtyMaterial);
}

QSGTexture::Filtering MaskEffectNode::mipmapFiltering() const
{
    return m_material.mipmapFiltering();
}

void MaskEffectNode::setAnisotropyLevel(QSGTexture::AnisotropyLevel level)
{
    if (m_material.anisotropyLevel() == level)
        return;

    m_material.setAnisotropyLevel(level);
    m_opaque_material.setAnisotropyLevel(level);
    markDirty(DirtyMaterial);
}

void MaskEffectNode::setMaskTexture(QSGTexture *texture)
{
    if (texture == m_material.maskTexture())
        return;

    m_material.setMaskTexture(texture);
    m_opaque_material.setMaskTexture(texture);
    markDirty(DirtyMaterial);
}

void MaskEffectNode::setMaskScale(QVector2D maskScale)
{
    if (maskScale == m_material.maskScale())
        return;

    m_material.setMaskScale(maskScale);
    m_opaque_material.setMaskScale(maskScale);
    markDirty(DirtyMaterial);
}

void MaskEffectNode::setMaskOffset(QVector2D maskOffset)
{
    if (m_material.maskOffset() == maskOffset)
        return;

    m_material.setMaskOffset(maskOffset);
    m_opaque_material.setMaskOffset(maskOffset);
    markDirty(DirtyMaterial);
}

void MaskEffectNode::setSourceScale(QVector2D sourceScale)
{
    if (sourceScale == m_material.sourceScale())
        return;

    m_material.setSourceScale(sourceScale);
    m_opaque_material.setSourceScale(sourceScale);
    markDirty(DirtyMaterial);
}

QSGTexture::AnisotropyLevel MaskEffectNode::anisotropyLevel() const
{
    return m_material.anisotropyLevel();
}

void MaskEffectNode::setRect(const QRectF &r)
{
    if (m_rect == r)
        return;

    m_rect = r;
    rebuildGeometry(&m_geometry, texture(), m_rect, m_sourceRect, m_texCoordMode);
    markDirty(DirtyGeometry);
}

QRectF MaskEffectNode::rect() const
{
    return m_rect;
}

void MaskEffectNode::setSourceRect(const QRectF &r)
{
    if (m_sourceRect == r)
        return;

    m_sourceRect = r;
    rebuildGeometry(&m_geometry, texture(), m_rect, m_sourceRect, m_texCoordMode);
    markDirty(DirtyGeometry);
}

QRectF MaskEffectNode::sourceRect() const
{
    return m_sourceRect;
}

void MaskEffectNode::setTexture(QSGTexture *texture)
{
    Q_ASSERT(texture);
    DirtyState dirty = DirtyMaterial;
    if (m_material.texture() == texture) {
        markDirty(dirty);
        return;
    }

    if (m_ownsTexture)
        delete m_material.texture();
    m_material.setTexture(texture);
    m_opaque_material.setTexture(texture);
    rebuildGeometry(&m_geometry, texture, m_rect, m_sourceRect, m_texCoordMode);

    bool wasAtlas = m_isAtlasTexture;
    m_isAtlasTexture = texture->isAtlasTexture();
    if (wasAtlas || m_isAtlasTexture)
        dirty |= DirtyGeometry;
    markDirty(dirty);
}

QSGTexture *MaskEffectNode::texture() const
{
    return m_material.texture();
}

void MaskEffectNode::setTextureCoordinatesTransform(TextureCoordinatesTransformMode mode)
{
    if (m_texCoordMode == mode)
        return;
    m_texCoordMode = mode;
    rebuildGeometry(&m_geometry, texture(), m_rect, m_sourceRect, m_texCoordMode);
    markDirty(DirtyMaterial);
}

QSGDefaultImageNode::TextureCoordinatesTransformMode MaskEffectNode::textureCoordinatesTransform() const
{
    return m_texCoordMode;
}

void MaskEffectNode::setOwnsTexture(bool owns)
{
    m_ownsTexture = owns;
}

bool MaskEffectNode::ownsTexture() const
{
    return m_ownsTexture;
}

QSGMaterialType *TextureMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}

QSGMaterialShader *TextureMaterial::createShader() const
{
    return new TextureMaterialShader;
}

int TextureMaterial::compare(const QSGMaterial *o) const
{
    return OpaqueTextureMaterial::compare(o);
}

QSGMaterialType *OpaqueTextureMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}

QSGMaterialShader *OpaqueTextureMaterial::createShader() const
{
    return new OpaqueTextureMaterialShader;
}

int OpaqueTextureMaterial::compare(const QSGMaterial *o) const
{
    Q_ASSERT(o && type() == o->type());
    return Q_UNLIKELY(o == this) ? 0 : 1;
}

void OpaqueTextureMaterial::setMaskTexture(QSGTexture *texture)
{
    Q_ASSERT(texture);
    if (!m_maskTexture) {
        m_maskTexture = texture;
        return;
    }

    if (texture->textureId() == m_maskTexture->textureId())
        return;

    m_maskTexture = texture;
}

void OpaqueTextureMaterial::setMaskScale(QVector2D maskScale)
{
    if (maskScale == m_maskScale)
        return;

    m_maskScale = maskScale;
}

void OpaqueTextureMaterial::setMaskOffset(QVector2D maskOffset)
{
    if (maskOffset == m_maskOffset)
        return;

    m_maskOffset = maskOffset;
}

void OpaqueTextureMaterial::setSourceScale(QVector2D sourceScale)
{
    if (sourceScale == m_sourceScale)
        return;

    m_sourceScale = sourceScale;
}

DQUICK_END_NAMESPACE
