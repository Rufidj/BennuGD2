#ifndef EFFECTGENERATOR_H
#define EFFECTGENERATOR_H

#include <QImage>
#include <QColor>
#include <QVector>
#include <QPointF>
#include <QRandomGenerator>
#include <cmath>

enum EffectType {
    EFFECT_EXPLOSION,
    EFFECT_SMOKE,
    EFFECT_FIRE,
    EFFECT_PARTICLES,
    EFFECT_WATER,
    EFFECT_ENERGY,
    EFFECT_IMPACT
};

struct Particle {
    QPointF position;
    QPointF prevPosition;  // For motion trails
    QPointF velocity;
    QColor color;
    float size;
    float life;         // 0.0 - 1.0
    float alpha;
    float rotation;
    float angularVel;
    float temperature;  // 0.0-1.0, for fire/heat effects
    int type;          // Sub-type (0=normal, 1=core, 2=spark, 3=smoke)
    
    Particle() : size(1.0f), life(1.0f), alpha(1.0f), rotation(0.0f), angularVel(0.0f), 
                 temperature(0.5f), type(0) {}
};

struct EffectParams {
    // General
    int frames;
    int imageSize;
    int seed;
    
    // Common
    int particleCount;
    float intensity;
    float speed;
    QColor color1;
    QColor color2;
    
    // Specific
    float radius;
    float turbulence;
    float gravity;
    float dispersion;
    float fadeRate;
    bool debris;
    bool sparks;
    bool trails;
    
    EffectParams() 
        : frames(30), imageSize(128), seed(0)
        , particleCount(100), intensity(50.0f), speed(10.0f)
        , color1(Qt::white), color2(Qt::black)
        , radius(50.0f), turbulence(0.5f), gravity(0.0f)
        , dispersion(1.0f), fadeRate(0.05f)
        , debris(false), sparks(false), trails(false) {}
};

class EffectGenerator
{
public:
    EffectGenerator();
    
    void setType(EffectType type);
    void setParams(const EffectParams &params);
    
    QVector<QImage> generateAnimation();
    QImage renderFrame(int frameIndex, float time);
    
private:
    void initializeParticles();
    void updateParticles(float time, float deltaTime);
    
    // Effect-specific initialization
    void initExplosion();
    void initSmoke();
    void initFire();
    void initParticles();
    void initWater();
    void initEnergy();
    void initImpact();
    
    // Effect-specific updates
    void updateExplosion(float time, float dt);
    void updateSmoke(float time, float dt);
    void updateFire(float time, float dt);
    void updateParticlesGeneric(float time, float dt);
    void updateWater(float time, float dt);
    void updateEnergy(float time, float dt);
    void updateImpact(float time, float dt);
    
    // Rendering helpers
    void drawGlowParticle(QPainter &painter, const Particle &p, const QPointF &center);
    void drawFireParticle(QPainter &painter, const Particle &p, const QPointF &center);
    void drawExplosionParticle(QPainter &painter, const Particle &p, const QPointF &center);
    void drawSmokeParticle(QPainter &painter, const Particle &p, const QPointF &center);
    
    // Utilities
    float perlinNoise(float x, float y);
    QColor lerpColor(const QColor &a, const QColor &b, float t);
    float randomFloat(float min, float max);
    QColor temperatureToColor(float temp);  // Convert temperature to realistic fire color
    
    EffectType m_type;
    EffectParams m_params;
    QVector<Particle> m_particles;
    QRandomGenerator m_random;
};

#endif // EFFECTGENERATOR_H
