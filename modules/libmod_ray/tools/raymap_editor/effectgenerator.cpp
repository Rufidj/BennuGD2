#include "effectgenerator.h"
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <QDateTime>
#include <algorithm>

EffectGenerator::EffectGenerator()
    : m_type(EFFECT_EXPLOSION)
{
    m_random.seed(QDateTime::currentMSecsSinceEpoch());
}

void EffectGenerator::setType(EffectType type)
{
    m_type = type;
}

void EffectGenerator::setParams(const EffectParams &params)
{
    m_params = params;
    if (m_params.seed != 0) {
        m_random.seed(m_params.seed);
    }
}

QVector<QImage> EffectGenerator::generateAnimation()
{
    QVector<QImage> frames;
    initializeParticles();
    
    for (int i = 0; i < m_params.frames; i++) {
        float time = (float)i / m_params.frames;
        float deltaTime = 1.0f / m_params.frames;
        frames.append(renderFrame(i, time));
    }
    
    return frames;
}

QImage EffectGenerator::renderFrame(int frameIndex, float time)
{
    QImage frame(m_params.imageSize, m_params.imageSize, QImage::Format_ARGB32);
    frame.fill(Qt::transparent);
    
    QPainter painter(&frame);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    
    // ADDITIVE BLENDING for realistic glow/fire
    painter.setCompositionMode(QPainter::CompositionMode_Plus);
    
    float deltaTime = 1.0f / m_params.frames;
    updateParticles(time, deltaTime);
    
    QPointF center(m_params.imageSize / 2.0f, m_params.imageSize / 2.0f);
    
    // Sort particles by type for proper layering (smoke behind, cores in front)
    QVector<const Particle*> sorted;
    for (const Particle &p : m_particles) {
        if (p.life > 0 && p.alpha > 0) {
            sorted.append(&p);
        }
    }
    std::sort(sorted.begin(), sorted.end(), [](const Particle *a, const Particle *b) {
        return a->type > b->type; // Smoke (3) first, cores (1) last
    });
    
    // Draw particles with effect-specific rendering
    for (const Particle *p : sorted) {
        switch (m_type) {
            case EFFECT_FIRE:
                drawFireParticle(painter, *p, center);
                break;
            case EFFECT_EXPLOSION:
                drawExplosionParticle(painter, *p, center);
                break;
            case EFFECT_SMOKE:
                drawSmokeParticle(painter, *p, center);
                break;
            default:
                drawGlowParticle(painter, *p, center);
                break;
        }
    }
    
    return frame;
}

void EffectGenerator::initializeParticles()
{
    m_particles.clear();
    
    switch (m_type) {
        case EFFECT_EXPLOSION: initExplosion(); break;
        case EFFECT_SMOKE: initSmoke(); break;
        case EFFECT_FIRE: initFire(); break;
        case EFFECT_PARTICLES: initParticles(); break;
        case EFFECT_WATER: initWater(); break;
        case EFFECT_ENERGY: initEnergy(); break;
        case EFFECT_IMPACT: initImpact(); break;
    }
}

void EffectGenerator::updateParticles(float time, float deltaTime)
{
    switch (m_type) {
        case EFFECT_EXPLOSION: updateExplosion(time, deltaTime); break;
        case EFFECT_SMOKE: updateSmoke(time, deltaTime); break;
        case EFFECT_FIRE: updateFire(time, deltaTime); break;
        case EFFECT_PARTICLES: updateParticlesGeneric(time, deltaTime); break;
        case EFFECT_WATER: updateWater(time, deltaTime); break;
        case EFFECT_ENERGY: updateEnergy(time, deltaTime); break;
        case EFFECT_IMPACT: updateImpact(time, deltaTime); break;
    }
}

// EXPLOSION
void EffectGenerator::initExplosion()
{
    int count = m_params.particleCount;
    
    // Bright core flash (initial white-hot center)
    for (int i = 0; i < 15; i++) {
        Particle p;
        p.position = QPointF(randomFloat(-3, 3), randomFloat(-3, 3));
        p.prevPosition = p.position;
        p.velocity = QPointF(0, 0);
        p.size = randomFloat(15, 25);
        p.life = 1.0f;
        p.alpha = 1.0f;
        p.type = 1;  // Core flash
        p.temperature = 1.0f;
        p.color = QColor(255, 255, 255);
        m_particles.append(p);
    }
    
    // Expanding fireball
    for (int i = 0; i < count * 0.6; i++) {
        Particle p;
        float angle = randomFloat(0, 2 * M_PI);
        float speed = randomFloat(0.5f, 1.2f) * m_params.speed;
        
        p.position = QPointF(randomFloat(-5, 5), randomFloat(-5, 5));
        p.prevPosition = p.position;
        p.velocity = QPointF(cos(angle) * speed, sin(angle) * speed);
        p.size = randomFloat(3, 8) * (m_params.intensity / 50.0f);
        p.life = 1.0f;
        p.alpha = 1.0f;
        p.type = 0;  // Fireball
        p.temperature = randomFloat(0.7f, 1.0f);
        p.color = temperatureToColor(p.temperature);
        
        m_particles.append(p);
    }
    
    // Smoke (appears later, persists longer)
    for (int i = 0; i < count * 0.4; i++) {
        Particle p;
        float angle = randomFloat(0, 2 * M_PI);
        float speed = randomFloat(0.3f, 0.8f) * m_params.speed;
        
        p.position = QPointF(randomFloat(-8, 8), randomFloat(-8, 8));
        p.prevPosition = p.position;
        p.velocity = QPointF(cos(angle) * speed, sin(angle) * speed);
        p.size = randomFloat(5, 12);
        p.life = 1.2f;  // Lives longer than fire
        p.alpha = 0.0f;  // Starts invisible, fades in
        p.type = 3;  // Smoke
        p.color = QColor(40, 40, 40);
        
        m_particles.append(p);
    }
}

void EffectGenerator::updateExplosion(float time, float dt)
{
    for (Particle &p : m_particles) {
        p.prevPosition = p.position;
        
        if (p.type == 1) {  // Core flash
            // Rapid expansion and fade
            p.size *= 1.15f;
            p.alpha = (1.0f - time) * (1.0f - time);  // Quadratic fade
            p.life = 1.0f - time * 3;  // Fades quickly
            
        } else if (p.type == 0) {  // Fireball
            p.position += p.velocity * m_params.radius * 0.12f;
            
            // Cool down as it expands
            p.temperature -= dt * 0.8f;
            p.temperature = qMax(0.0f, p.temperature);
            p.color = temperatureToColor(p.temperature);
            
            p.life = 1.0f - time;
            p.alpha = p.life;
            p.size *= 0.98f;
            
        } else if (p.type == 3) {  // Smoke
            p.position += p.velocity * m_params.radius * 0.08f;
            
            // Fade in during first 30% of animation
            if (time < 0.3f) {
                p.alpha = time / 0.3f * 0.7f;
            } else {
                // Then slowly fade out
                p.alpha = 0.7f * (1.0f - (time - 0.3f) / 0.7f);
            }
            
            // Expand
            p.size += 0.3f * m_params.dispersion;
            
            // Turbulence
            float noise = perlinNoise(p.position.x() * 0.05f, time * 3);
            p.position += QPointF(noise * 3, noise * 2);
            
            p.life = 1.2f - time;
        }
    }
}

// SMOKE
void EffectGenerator::initSmoke()
{
    int count = m_params.particleCount;
    for (int i = 0; i < count; i++) {
        Particle p;
        float offsetX = randomFloat(-20, 20);
        float offsetY = randomFloat(0, 10);
        
        p.position = QPointF(offsetX, offsetY);
        p.velocity = QPointF(randomFloat(-0.5f, 0.5f), -randomFloat(1, 3));
        p.size = randomFloat(5, 15);
        p.life = randomFloat(0.5f, 1.0f);
        p.alpha = randomFloat(0.3f, 0.7f);
        p.color = m_params.color1;
        
        m_particles.append(p);
    }
}

void EffectGenerator::updateSmoke(float time, float dt)
{
    for (Particle &p : m_particles) {
        // Rise and expand
        p.position += p.velocity * m_params.speed * 0.5f;
        p.size += 0.2f * m_params.dispersion;
        
        // Turbulence
        float noise = perlinNoise(p.position.x() * 0.1f, time * 5);
        p.position.rx() += noise * m_params.turbulence * 2;
        
        // Fade
        p.alpha -= m_params.fadeRate;
        p.life -= dt * 2;
    }
}

// FIRE
void EffectGenerator::initFire()
{
    int count = m_params.particleCount;
    
    // Main fire particles
    for (int i = 0; i < count * 0.7; i++) {
        Particle p;
        float offsetX = randomFloat(-15, 15);
        
        p.position = QPointF(offsetX, randomFloat(0, 20));
        p.prevPosition = p.position;
        p.velocity = QPointF(randomFloat(-0.3f, 0.3f), -randomFloat(2, 5));
        p.size = randomFloat(4, 10);
        p.life = randomFloat(0.5f, 1.0f);
        p.alpha = randomFloat(0.7f, 1.0f);
        p.temperature = randomFloat(0.6f, 1.0f);  // Hot fire
        p.type = 0;  // Normal fire particle
        p.color = temperatureToColor(p.temperature);
        
        m_particles.append(p);
    }
    
    // Smoke particles (rise from fire)
    for (int i = 0; i < count * 0.2; i++) {
        Particle p;
        p.position = QPointF(randomFloat(-20, 20), randomFloat(10, 30));
        p.prevPosition = p.position;
        p.velocity = QPointF(randomFloat(-0.5f, 0.5f), -randomFloat(1, 2));
        p.size = randomFloat(6, 12);
        p.life = randomFloat(0.3f, 0.8f);
        p.alpha = randomFloat(0.3f, 0.6f);
        p.type = 3;  // Smoke
        p.color = QColor(60, 60, 60);
        
        m_particles.append(p);
    }
    
    // Sparks (if enabled)
    if (m_params.sparks) {
        for (int i = 0; i < count * 0.1; i++) {
            Particle p;
            p.position = QPointF(randomFloat(-10, 10), randomFloat(0, 10));
            p.prevPosition = p.position;
            p.velocity = QPointF(randomFloat(-3, 3), -randomFloat(5, 12));
            p.size = randomFloat(1, 2);
            p.life = randomFloat(0.2f, 0.5f);
            p.alpha = 1.0f;
            p.type = 2;  // Spark
            p.temperature = 1.0f;  // Very hot
            p.color = QColor(255, 255, 200);
            
            m_particles.append(p);
        }
    }
}

void EffectGenerator::updateFire(float time, float dt)
{
    for (Particle &p : m_particles) {
        // Store previous position for trails
        p.prevPosition = p.position;
        
        p.position += p.velocity * m_params.speed * 0.3f;
        
        // Flicker using noise
        float flicker = perlinNoise(p.position.x() * 0.2f, time * 10);
        p.position.rx() += flicker * 2;
        
        if (p.type == 0) {  // Fire particles
            // Cool down as they rise (temperature decreases)
            p.temperature -= dt * 0.5f;
            p.temperature = qMax(0.0f, p.temperature);
            p.color = temperatureToColor(p.temperature);
            
            p.alpha -= 0.015f;
            p.life -= dt * 2.5f;
            p.size *= 0.99f;
            
        } else if (p.type == 2) {  // Sparks
            p.velocity.ry() += 0.15f;  // Gravity
            p.alpha -= 0.03f;
            p.life -= dt * 4;
            
        } else if (p.type == 3) {  // Smoke
            p.size += 0.15f * m_params.dispersion;
            float noise = perlinNoise(p.position.x() * 0.1f, time * 5);
            p.position.rx() += noise * m_params.turbulence * 2;
            p.alpha -= 0.01f;
            p.life -= dt * 1.5f;
        }
    }
    
    // Dynamically add new sparks during animation
    if (m_params.sparks && randomFloat(0, 1) > 0.8f && time < 0.7f) {
        Particle spark;
        spark.position = QPointF(randomFloat(-10, 10), randomFloat(0, 5));
        spark.prevPosition = spark.position;
        spark.velocity = QPointF(randomFloat(-2, 2), -randomFloat(6, 10));
        spark.size = randomFloat(1, 2);
        spark.life = 0.3f;
        spark.alpha = 1.0f;
        spark.type = 2;
        spark.temperature = 1.0f;
        spark.color = QColor(255, 255, 150);
        m_particles.append(spark);
    }
}

// PARTICLES (Generic)
void EffectGenerator::initParticles()
{
    int count = m_params.particleCount;
    for (int i = 0; i < count; i++) {
        Particle p;
        float angle = randomFloat(0, 2 * M_PI);
        float speed = randomFloat(0.5f, 2.0f);
        
        p.position = QPointF(0, 0);
        p.velocity = QPointF(cos(angle) * speed, sin(angle) * speed);
        p.size = randomFloat(2, 5);
        p.life = 1.0f;
        p.alpha = 1.0f;
        p.color = m_params.color1;
        
        m_particles.append(p);
    }
}

void EffectGenerator::updateParticlesGeneric(float time, float dt)
{
    for (Particle &p : m_particles) {
        p.position += p.velocity * m_params.speed * 0.5f;
        p.velocity.ry() += m_params.gravity * 0.1f; // Gravity
        p.life -= dt;
        p.alpha = p.life;
        
        // Color lerp
        p.color = lerpColor(m_params.color1, m_params.color2, 1.0f - p.life);
    }
}

// WATER
void EffectGenerator::initWater()
{
    int count = m_params.particleCount;
    for (int i = 0; i < count; i++) {
        Particle p;
        float angle = randomFloat(-M_PI/3, -2*M_PI/3); // Upward spray
        float speed = randomFloat(3, 8);
        
        p.position = QPointF(randomFloat(-5, 5), 0);
        p.velocity = QPointF(cos(angle) * speed, sin(angle) * speed);
        p.size = randomFloat(2, 4);
        p.life = 1.0f;
        p.alpha = 0.8f;
        p.color = QColor(100, 150, 255, 200);
        
        m_particles.append(p);
    }
}

void EffectGenerator::updateWater(float time, float dt)
{
    for (Particle &p : m_particles) {
        p.position += p.velocity * m_params.speed * 0.3f;
        p.velocity.ry() += 0.3f; // Gravity
        p.life -= dt * 2;
        p.alpha = p.life * 0.8f;
    }
}

// ENERGY
void EffectGenerator::initEnergy()
{
    int count = m_params.particleCount;
    for (int i = 0; i < count; i++) {
        Particle p;
        float angle = randomFloat(0, 2 * M_PI);
        float radius = randomFloat(0, m_params.radius);
        
        p.position = QPointF(cos(angle) * radius, sin(angle) * radius);
        p.velocity = QPointF(0, 0);
        p.size = randomFloat(2, 6);
        p.life = 1.0f;
        p.alpha = randomFloat(0.5f, 1.0f);
        p.color = m_params.color1;
        p.rotation = angle;
        p.angularVel = randomFloat(-0.1f, 0.1f);
        
        m_particles.append(p);
    }
}

void EffectGenerator::updateEnergy(float time, float dt)
{
    for (Particle &p : m_particles) {
        // Spiral motion
        p.rotation += p.angularVel;
        float radius = m_params.radius * (1.0f - time * 0.5f);
        p.position = QPointF(cos(p.rotation) * radius, sin(p.rotation) * radius);
        
        // Pulsate
        float pulse = sin(time * 10 + p.rotation) * 0.3f + 0.7f;
        p.alpha = pulse;
        p.size = 3 + pulse * 3;
        
        // Color shift
        p.color = lerpColor(m_params.color1, m_params.color2, sin(time * 5) * 0.5f + 0.5f);
    }
}

// IMPACT
void EffectGenerator::initImpact()
{
    int count = m_params.particleCount;
    
    // Dust cloud
    for (int i = 0; i < count / 2; i++) {
        Particle p;
        float angle = randomFloat(-M_PI/4, -3*M_PI/4);
        float speed = randomFloat(1, 4);
        
        p.position = QPointF(randomFloat(-10, 10), 0);
        p.velocity = QPointF(cos(angle) * speed, sin(angle) * speed);
        p.size = randomFloat(3, 8);
        p.life = 1.0f;
        p.alpha = 0.6f;
        p.color = QColor(150, 130, 100);
        
        m_particles.append(p);
    }
    
    // Debris
    if (m_params.debris) {
        for (int i = 0; i < count / 2; i++) {
            Particle p;
            float angle = randomFloat(0, 2 * M_PI);
            float speed = randomFloat(2, 6);
            
            p.position = QPointF(0, 0);
            p.velocity = QPointF(cos(angle) * speed, sin(angle) * speed);
            p.size = randomFloat(1, 3);
            p.life = 1.0f;
            p.alpha = 1.0f;
            p.color = QColor(80, 70, 60);
            
            m_particles.append(p);
        }
    }
}

void EffectGenerator::updateImpact(float time, float dt)
{
    for (Particle &p : m_particles) {
        p.position += p.velocity * m_params.speed * 0.4f;
        
        // Gravity for debris
        if (p.size < 4) {
            p.velocity.ry() += 0.2f;
        }
        
        // Expand dust
        if (p.size > 4) {
            p.size += 0.3f;
        }
        
        p.alpha -= 0.015f;
        p.life -= dt * 1.5f;
    }
}

// REALISTIC RENDERING FUNCTIONS

void EffectGenerator::drawGlowParticle(QPainter &painter, const Particle &p, const QPointF &center)
{
    QPointF pos = center + p.position;
    
    // Outer glow (large, soft)
    QRadialGradient outerGlow(pos, p.size * 2.0f);
    QColor glowColor = p.color;
    glowColor.setAlphaF(0);
    QColor coreColor = p.color;
    coreColor.setAlphaF(p.alpha * 0.3f);
    
    outerGlow.setColorAt(0, coreColor);
    outerGlow.setColorAt(0.5, glowColor);
    outerGlow.setColorAt(1, glowColor);
    
    painter.setBrush(outerGlow);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(pos, p.size * 2.0f, p.size * 2.0f);
    
    // Inner core (bright)
    QRadialGradient core(pos, p.size);
    QColor brightCore = p.color.lighter(150);
    brightCore.setAlphaF(p.alpha);
    coreColor.setAlphaF(p.alpha * 0.7f);
    
    core.setColorAt(0, brightCore);
    core.setColorAt(0.7, coreColor);
    core.setColorAt(1, glowColor);
    
    painter.setBrush(core);
    painter.drawEllipse(pos, p.size, p.size);
}

void EffectGenerator::drawFireParticle(QPainter &painter, const Particle &p, const QPointF &center)
{
    QPointF pos = center + p.position;
    
    if (p.type == 3) { // Smoke
        drawSmokeParticle(painter, p, center);
        return;
    }
    
    if (p.type == 2) { // Spark
        // Bright spark with trail
        QColor sparkColor(255, 255, 200, (int)(p.alpha * 255));
        painter.setBrush(sparkColor);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(pos, p.size * 0.5f, p.size * 0.5f);
        
        // Trail
        if (p.prevPosition != QPointF(0, 0)) {
            QPointF prevPos = center + p.prevPosition;
            QLinearGradient trail(pos, prevPos);
            trail.setColorAt(0, sparkColor);
            QColor transparent = sparkColor;
            transparent.setAlpha(0);
            trail.setColorAt(1, transparent);
            painter.setBrush(trail);
            
            QPolygonF trailPoly;
            QPointF dir = (pos - prevPos);
            QPointF perp(-dir.y(), dir.x());
            float len = sqrt(perp.x()*perp.x() + perp.y()*perp.y());
            if (len > 0.001f) {
                perp = perp / len * p.size * 0.3f;
                trailPoly << pos + perp << pos - perp << prevPos;
                painter.drawPolygon(trailPoly);
            }
        }
        return;
    }
    
    // REALISTIC FLAME SHAPE
    // Fire particle with temperature-based color
    QColor fireColor = temperatureToColor(p.temperature);
    
    float width = p.size;
    float height = p.size * 2.0f;  // Flames are tall
    
    // Create flame shape using distorted polygon
    QPolygonF flameShape;
    int segments = 12;
    
    for (int i = 0; i <= segments; i++) {
        float angle = M_PI * i / segments;  // Half circle (top of flame)
        float radius = width;
        
        // Add procedural distortion for flickering effect
        float noiseX = perlinNoise(pos.x() * 0.1f + i * 0.5f, p.life * 10);
        float noiseY = perlinNoise(pos.y() * 0.1f + i * 0.3f, p.life * 8);
        
        // Distort radius (more at top, less at bottom)
        float distortAmount = (1.0f - cos(angle)) * 0.3f;  // More distortion at top
        radius *= (1.0f + noiseX * distortAmount);
        
        float x = pos.x() + cos(angle + M_PI) * radius;
        float y = pos.y() - height * (1.0f - sin(angle)) + noiseY * height * 0.15f;
        
        flameShape << QPointF(x, y);
    }
    
    // Add pointed tip at top (characteristic flame shape)
    float tipNoise = perlinNoise(pos.x() * 0.15f, p.life * 12);
    flameShape << QPointF(pos.x() + tipNoise * width * 0.2f, pos.y() - height * 1.1f);
    
    // Close the shape at the base
    flameShape << QPointF(pos.x() + width, pos.y());
    flameShape << QPointF(pos.x() - width, pos.y());
    
    // Draw outer glow (soft, diffuse)
    QPainterPath glowPath;
    glowPath.addPolygon(flameShape);
    
    QRadialGradient outerGlow(pos.x(), pos.y() - height * 0.5f, height);
    QColor transparent = fireColor;
    transparent.setAlpha(0);
    QColor dimGlow = fireColor.darker(130);
    dimGlow.setAlphaF(p.alpha * 0.15f);
    
    outerGlow.setColorAt(0, dimGlow);
    outerGlow.setColorAt(0.7, transparent);
    outerGlow.setColorAt(1, transparent);
    
    painter.setBrush(outerGlow);
    painter.setPen(Qt::NoPen);
    
    // Draw glow slightly larger
    painter.save();
    painter.translate(pos);
    painter.scale(1.3, 1.3);
    painter.translate(-pos);
    painter.drawPath(glowPath);
    painter.restore();
    
    // Draw main flame body with gradient
    QLinearGradient flameGradient(pos.x(), pos.y(), pos.x(), pos.y() - height);
    
    // Hot core at base
    QColor hotCore = temperatureToColor(qMin(p.temperature + 0.3f, 1.0f));
    hotCore.setAlphaF(p.alpha * 0.9f);
    
    // Cooler at tip
    QColor coolTip = temperatureToColor(qMax(p.temperature - 0.2f, 0.0f));
    coolTip.setAlphaF(p.alpha * 0.6f);
    
    flameGradient.setColorAt(0, hotCore);
    flameGradient.setColorAt(0.5, fireColor);
    flameGradient.setColorAt(1, coolTip);
    
    painter.setBrush(flameGradient);
    painter.drawPolygon(flameShape);
    
    // Add bright inner core (white-hot center at base)
    if (p.temperature > 0.7f) {
        QRadialGradient innerCore(pos.x(), pos.y() - height * 0.1f, width * 0.6f);
        QColor white(255, 255, 255, (int)(p.alpha * 180));
        QColor yellowCore(255, 255, 180, (int)(p.alpha * 120));
        transparent = yellowCore;
        transparent.setAlpha(0);
        
        innerCore.setColorAt(0, white);
        innerCore.setColorAt(0.4, yellowCore);
        innerCore.setColorAt(1, transparent);
        
        painter.setBrush(innerCore);
        painter.drawEllipse(QPointF(pos.x(), pos.y() - height * 0.1f), width * 0.6f, height * 0.2f);
    }
}

void EffectGenerator::drawExplosionParticle(QPainter &painter, const Particle &p, const QPointF &center)
{
    QPointF pos = center + p.position;
    
    if (p.type == 1) { // Core flash
        // Extremely bright white core
        QRadialGradient flash(pos, p.size);
        QColor white(255, 255, 255, (int)(p.alpha * 255));
        QColor yellow(255, 255, 100, (int)(p.alpha * 200));
        QColor transparent(255, 200, 0, 0);
        
        flash.setColorAt(0, white);
        flash.setColorAt(0.3, yellow);
        flash.setColorAt(0.7, transparent);
        flash.setColorAt(1, transparent);
        
        painter.setBrush(flash);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(pos, p.size, p.size);
        return;
    }
    
    if (p.type == 3) { // Smoke
        drawSmokeParticle(painter, p, center);
        return;
    }
    
    // Fireball particle
    QRadialGradient fireball(pos, p.size * 1.5f);
    QColor core = p.color.lighter(180);
    core.setAlphaF(p.alpha);
    QColor mid = p.color;
    mid.setAlphaF(p.alpha * 0.7f);
    QColor outer = p.color.darker(120);
    outer.setAlphaF(p.alpha * 0.3f);
    QColor transparent = outer;
    transparent.setAlpha(0);
    
    fireball.setColorAt(0, core);
    fireball.setColorAt(0.4, mid);
    fireball.setColorAt(0.7, outer);
    fireball.setColorAt(1, transparent);
    
    painter.setBrush(fireball);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(pos, p.size * 1.5f, p.size * 1.5f);
}

void EffectGenerator::drawSmokeParticle(QPainter &painter, const Particle &p, const QPointF &center)
{
    QPointF pos = center + p.position;
    
    // Soft, wispy smoke with very diffuse edges
    QRadialGradient smoke(pos, p.size * 1.2f);
    QColor smokeColor = p.color;
    smokeColor.setAlphaF(p.alpha * 0.6f);  // Semi-transparent
    QColor smokeMid = p.color.lighter(110);
    smokeMid.setAlphaF(p.alpha * 0.4f);
    QColor transparent = smokeColor;
    transparent.setAlpha(0);
    
    smoke.setColorAt(0, smokeColor);
    smoke.setColorAt(0.4, smokeMid);
    smoke.setColorAt(0.8, transparent);
    smoke.setColorAt(1, transparent);
    
    painter.setBrush(smoke);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(pos, p.size * 1.2f, p.size * 1.2f);
}

QColor EffectGenerator::temperatureToColor(float temp)
{
    // Realistic blackbody radiation colors for fire
    // temp: 0.0 = cool (red), 0.5 = medium (orange), 1.0 = hot (white)
    
    if (temp < 0.33f) {
        // Dark red to bright red
        float t = temp / 0.33f;
        return lerpColor(QColor(100, 20, 0), QColor(255, 50, 0), t);
    } else if (temp < 0.66f) {
        // Red to orange-yellow
        float t = (temp - 0.33f) / 0.33f;
        return lerpColor(QColor(255, 50, 0), QColor(255, 180, 0), t);
    } else {
        // Orange to white-hot
        float t = (temp - 0.66f) / 0.34f;
        return lerpColor(QColor(255, 180, 0), QColor(255, 255, 220), t);
    }
}

// UTILITIES
float EffectGenerator::perlinNoise(float x, float y)
{
    // Simple noise approximation
    int xi = (int)x;
    int yi = (int)y;
    float xf = x - xi;
    float yf = y - yi;
    
    float n00 = sin(xi * 12.9898f + yi * 78.233f) * 43758.5453f;
    float n10 = sin((xi+1) * 12.9898f + yi * 78.233f) * 43758.5453f;
    float n01 = sin(xi * 12.9898f + (yi+1) * 78.233f) * 43758.5453f;
    float n11 = sin((xi+1) * 12.9898f + (yi+1) * 78.233f) * 43758.5453f;
    
    n00 = n00 - floor(n00);
    n10 = n10 - floor(n10);
    n01 = n01 - floor(n01);
    n11 = n11 - floor(n11);
    
    float nx0 = n00 * (1 - xf) + n10 * xf;
    float nx1 = n01 * (1 - xf) + n11 * xf;
    
    return nx0 * (1 - yf) + nx1 * yf;
}

QColor EffectGenerator::lerpColor(const QColor &a, const QColor &b, float t)
{
    t = qBound(0.0f, t, 1.0f);
    return QColor(
        a.red() + (b.red() - a.red()) * t,
        a.green() + (b.green() - a.green()) * t,
        a.blue() + (b.blue() - a.blue()) * t,
        a.alpha() + (b.alpha() - a.alpha()) * t
    );
}

float EffectGenerator::randomFloat(float min, float max)
{
    return min + (max - min) * (m_random.generateDouble());
}
