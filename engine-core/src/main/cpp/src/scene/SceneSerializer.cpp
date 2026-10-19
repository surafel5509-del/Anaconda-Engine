// ===========================================================================
//  scene/SceneSerializer.cpp — v2 encode/decode (host-testable).
// ===========================================================================
#include "anaconda/scene/SceneSerializer.hpp"
#include "anaconda/core/Log.hpp"
#include "anaconda/ecs/Components.hpp"
#include <cstdio>
#include <cstring>
namespace anaconda::scene {
namespace {
struct Writer {
    std::vector<std::uint8_t> out;
    void u32(std::uint32_t v) { out.push_back(v&0xFFu); out.push_back((v>>8)&0xFFu); out.push_back((v>>16)&0xFFu); out.push_back((v>>24)&0xFFu); }
    void i32(std::int32_t v) { u32(static_cast<std::uint32_t>(v)); }
    void f32(float v) { std::uint32_t b=0; std::memcpy(&b,&v,4); u32(b); }
    void u8(std::uint8_t v) { out.push_back(v); }
    void str(const std::string& s) { u32(static_cast<std::uint32_t>(s.size())); out.insert(out.end(),s.begin(),s.end()); }
    void vec3(const Vec3& v) { f32(v.x); f32(v.y); f32(v.z); }
    void vec4(const Vec4& v) { f32(v.x); f32(v.y); f32(v.z); f32(v.w); }
};
struct Reader {
    const std::vector<std::uint8_t>& in; std::size_t pos=0; std::string err;
    bool fail(const std::string& why){ if(err.empty()) err=why; return false; }
    bool need(std::size_t n){ if(!err.empty()) return false; if(pos+n>in.size()) return fail("truncated at "+std::to_string(pos)); return true; }
    bool u32(std::uint32_t& v){ if(!need(4)) return false; v=in[pos]|(in[pos+1]<<8)|(in[pos+2]<<16)|(in[pos+3]<<24); pos+=4; return true; }
    bool i32(std::int32_t& v){ std::uint32_t r=0; if(!u32(r)) return false; std::memcpy(&v,&r,4); return true; }
    bool f32(float& v){ std::uint32_t b=0; if(!u32(b)) return false; std::memcpy(&v,&b,4); return true; }
    bool u8(std::uint8_t& v){ if(!need(1)) return false; v=in[pos++]; return true; }
    bool str(std::string& s){ std::uint32_t len=0; if(!u32(len)) return false; if(len>SceneSerializer::kMaxStringBytes) return fail("string too long"); if(!need(len)) return false; s.assign(reinterpret_cast<const char*>(in.data()+pos),len); pos+=len; return true; }
    bool vec3(Vec3& v){ return f32(v.x)&&f32(v.y)&&f32(v.z); }
    bool vec4(Vec4& v){ return f32(v.x)&&f32(v.y)&&f32(v.z)&&f32(v.w); }
};
}
std::vector<std::uint8_t> SceneSerializer::serialize(const ecs::Registry& world){
    Writer w; w.u32(kMagic); w.u32(kVersion);
    auto entities = world.aliveEntities(); w.u32(static_cast<std::uint32_t>(entities.size())); w.u32(0);
    for(ecs::Entity e: entities){
        auto* tag=world.tryGet<TagComponent>(e); auto* tf=world.tryGet<TransformComponent>(e);
        auto* sp=world.tryGet<SpriteComponent>(e); auto* rb=world.tryGet<RigidBodyComponent>(e);
        auto* col=world.tryGet<BoxColliderComponent>(e); auto* sc=world.tryGet<ScriptComponent>(e);
        auto* cam=world.tryGet<CameraComponent>(e); auto* anim=world.tryGet<SpriteAnimatorComponent>(e);
        auto* aud=world.tryGet<AudioSourceComponent>(e); auto* mat=world.tryGet<PhysicsMaterialComponent>(e);
        auto* pref=world.tryGet<PrefabComponent>(e);
        std::uint32_t flags=0;
        if(tag) flags|=kHasTag; if(tf) flags|=kHasTransform; if(sp) flags|=kHasSprite;
        if(rb) flags|=kHasRigidBody; if(col) flags|=kHasCollider; if(sc) flags|=kHasScript;
        if(cam) flags|=kHasCamera; if(anim) flags|=kHasAnimator; if(aud) flags|=kHasAudioSource;
        if(mat) flags|=kHasPhysicsMaterial; if(pref) flags|=kHasPrefab;
        w.u32(flags);
        if(tag) w.str(tag->name);
        if(tf){ w.vec3(tf->position); w.vec3(tf->rotation); w.vec3(tf->scale); w.u32(tf->parent); }
        if(sp){ w.vec4(sp->color); w.u32(sp->textureId); w.f32(sp->size.x); w.f32(sp->size.y); w.i32(sp->renderLayer); w.vec4(sp->uvRect); }
        if(rb){ w.vec3(rb->velocity); w.f32(rb->gravityScale); w.f32(rb->mass); w.u8(rb->useGravity?1:0); w.u8(rb->isStatic?1:0); w.u8(rb->grounded?1:0); w.u8(rb->isKinematic?1:0); w.f32(rb->friction); w.f32(rb->restitution); w.f32(rb->linearDamping); w.f32(rb->angularDamping); }
        if(col){ w.vec3(col->halfExtents); w.vec3(col->center); w.u8(col->isTrigger?1:0); w.u8(0); w.u8(0); w.u8(0); w.f32(col->friction); w.f32(col->restitution); }
        if(sc){ w.str(sc->assetPath); w.u8(sc->enabled?1:0); w.u8(0); w.u8(0); w.u8(0); w.u32(sc->luaEnvRef); w.str(sc->lastError); }
        if(cam){ w.f32(cam->fieldOfViewDeg); w.f32(cam->orthoHalfHeight); w.f32(cam->zNear); w.f32(cam->zFar); w.u8(cam->primary?1:0); w.u8(0); w.u8(0); w.u8(0); w.f32(cam->shakeAmount); w.f32(cam->shakeDuration); }
        if(anim){ w.str(anim->currentClip); w.f32(anim->speed); w.f32(anim->time); w.i32(anim->currentFrame); w.u8(anim->playing?1:0); w.u8(anim->loop?1:0); w.u8(0); w.u8(0); }
        if(aud){ w.str(aud->clipPath); w.f32(aud->volume); w.f32(aud->pitch); w.u8(aud->loop?1:0); w.u8(aud->playOnAwake?1:0); w.u8(0); w.u8(0); }
        if(mat){ w.f32(mat->friction); w.f32(mat->restitution); w.f32(mat->density); w.u8(0); w.u8(0); w.u8(0); w.u8(0); }
        if(pref){ w.str(pref->prefabPath); w.u32(static_cast<std::uint32_t>(pref->overrides.size())); for(auto& kv: pref->overrides){ w.str(kv.first); w.str(kv.second); } }
    }
    return std::move(w.out);
}
bool SceneSerializer::deserialize(ecs::Registry& world, const std::vector<std::uint8_t>& data, std::string* error){
    auto setError=[&](const std::string& why){ if(error) *error=why; world.clear(); ANA_LOGE("[Scene] deserialize failed: %s", why.c_str()); return false; };
    world.clear();
    Reader r{data,0,{}}; std::uint32_t magic=0,version=0,count=0,reserved=0;
    if(!r.u32(magic)) return setError(r.err.empty()?"empty scene file":r.err);
    if(magic!=kMagic){ char hex[16]={0}; std::snprintf(hex,sizeof(hex),"0x%08X",magic); return setError(std::string("not an Anaconda scene (bad magic ")+hex+")"); }
    if(!r.u32(version)) return setError(r.err);
    if(version!=1 && version!=2) return setError("scene format v"+std::to_string(version)+" is not supported (engine reads v"+std::to_string(kVersion)+")");
    if(!r.u32(count)) return setError(r.err); if(!r.u32(reserved)) return setError(r.err);
    if(count>kMaxEntities) return setError("entity count "+std::to_string(count)+" exceeds safety limit");
    for(std::uint32_t i=0;i<count;++i){
        std::uint32_t flags=0; if(!r.u32(flags)) return setError(r.err);
        std::uint32_t known = kHasTag|kHasTransform|kHasSprite|kHasRigidBody|kHasCollider|kHasScript|kHasCamera|kHasAnimator|kHasAudioSource|kHasPhysicsMaterial|kHasPrefab;
        if(version==1) known = kHasTag|kHasTransform|kHasSprite|kHasRigidBody|kHasCollider|kHasScript|kHasCamera;
        if((flags & known)==0) return setError("entity "+std::to_string(i)+" has no component flags (corrupt stream)");
        ecs::Entity e=world.create();
        if(flags & kHasTag){ TagComponent t; if(!r.str(t.name)) return setError(r.err); world.add(e,std::move(t)); }
        if(flags & kHasTransform){ TransformComponent tf; if(!r.vec3(tf.position)||!r.vec3(tf.rotation)||!r.vec3(tf.scale)||!r.u32(tf.parent)) return setError(r.err); world.add(e,tf); }
        if(flags & kHasSprite){
            SpriteComponent sp;
            if(version==1){
                if(!r.vec4(sp.color)||!r.u32(sp.textureId)||!r.f32(sp.size.x)||!r.f32(sp.size.y)||!r.i32(sp.renderLayer)) return setError(r.err);
                sp.uvRect={0,0,1,1};
            } else {
                if(!r.vec4(sp.color)||!r.u32(sp.textureId)||!r.f32(sp.size.x)||!r.f32(sp.size.y)||!r.i32(sp.renderLayer)||!r.vec4(sp.uvRect)) return setError(r.err);
            }
            world.add(e,sp);
        }
        if(flags & kHasRigidBody){
            RigidBodyComponent rb; std::uint8_t b0=0,b1=0,b2=0,b3=0,pad=0;
            if(version==1){
                if(!r.vec3(rb.velocity)||!r.f32(rb.gravityScale)||!r.f32(rb.mass)||!r.u8(b0)||!r.u8(b1)||!r.u8(b2)||!r.u8(pad)) return setError(r.err);
                rb.useGravity=b0!=0; rb.isStatic=b1!=0; rb.grounded=b2!=0; rb.isKinematic=false; rb.friction=0.2f; rb.restitution=0; rb.linearDamping=0; rb.angularDamping=0.05f;
            } else {
                if(!r.vec3(rb.velocity)||!r.f32(rb.gravityScale)||!r.f32(rb.mass)||!r.u8(b0)||!r.u8(b1)||!r.u8(b2)||!r.u8(b3)||!r.f32(rb.friction)||!r.f32(rb.restitution)||!r.f32(rb.linearDamping)||!r.f32(rb.angularDamping)) return setError(r.err);
                rb.useGravity=b0!=0; rb.isStatic=b1!=0; rb.grounded=b2!=0; rb.isKinematic=b3!=0;
            }
            world.add(e,rb);
        }
        if(flags & kHasCollider){
            BoxColliderComponent col; std::uint8_t b0=0,p1=0,p2=0,p3=0;
            if(version==1){
                if(!r.vec3(col.halfExtents)||!r.vec3(col.center)||!r.u8(b0)||!r.u8(p1)||!r.u8(p2)||!r.u8(p3)) return setError(r.err);
                col.isTrigger=b0!=0; col.friction=0.2f; col.restitution=0;
            } else {
                if(!r.vec3(col.halfExtents)||!r.vec3(col.center)||!r.u8(b0)||!r.u8(p1)||!r.u8(p2)||!r.u8(p3)||!r.f32(col.friction)||!r.f32(col.restitution)) return setError(r.err);
                col.isTrigger=b0!=0;
            }
            world.add(e,col);
        }
        if(flags & kHasScript){
            ScriptComponent sc; std::uint8_t b0=0,p1=0,p2=0,p3=0;
            if(version==1){
                if(!r.str(sc.assetPath)||!r.u8(b0)||!r.u8(p1)||!r.u8(p2)||!r.u8(p3)||!r.u32(sc.luaEnvRef)) return setError(r.err);
                sc.enabled=b0!=0;
            } else {
                if(!r.str(sc.assetPath)||!r.u8(b0)||!r.u8(p1)||!r.u8(p2)||!r.u8(p3)||!r.u32(sc.luaEnvRef)||!r.str(sc.lastError)) return setError(r.err);
                sc.enabled=b0!=0;
            }
            world.add(e,std::move(sc));
        }
        if(flags & kHasCamera){
            CameraComponent cam; std::uint8_t b0=0,p1=0,p2=0,p3=0;
            if(version==1){
                if(!r.f32(cam.fieldOfViewDeg)||!r.f32(cam.orthoHalfHeight)||!r.f32(cam.zNear)||!r.f32(cam.zFar)||!r.u8(b0)||!r.u8(p1)||!r.u8(p2)||!r.u8(p3)) return setError(r.err);
                cam.primary=b0!=0;
            } else {
                if(!r.f32(cam.fieldOfViewDeg)||!r.f32(cam.orthoHalfHeight)||!r.f32(cam.zNear)||!r.f32(cam.zFar)||!r.u8(b0)||!r.u8(p1)||!r.u8(p2)||!r.u8(p3)||!r.f32(cam.shakeAmount)||!r.f32(cam.shakeDuration)) return setError(r.err);
                cam.primary=b0!=0;
            }
            world.add(e,cam);
        }
        if(flags & kHasAnimator){
            SpriteAnimatorComponent an; std::uint8_t b0=0,b1=0,p2=0,p3=0;
            if(!r.str(an.currentClip)||!r.f32(an.speed)||!r.f32(an.time)||!r.i32(an.currentFrame)||!r.u8(b0)||!r.u8(b1)||!r.u8(p2)||!r.u8(p3)) return setError(r.err);
            an.playing=b0!=0; an.loop=b1!=0; world.add(e,std::move(an));
        }
        if(flags & kHasAudioSource){
            AudioSourceComponent au; std::uint8_t b0=0,b1=0,p2=0,p3=0;
            if(!r.str(au.clipPath)||!r.f32(au.volume)||!r.f32(au.pitch)||!r.u8(b0)||!r.u8(b1)||!r.u8(p2)||!r.u8(p3)) return setError(r.err);
            au.loop=b0!=0; au.playOnAwake=b1!=0; world.add(e,std::move(au));
        }
        if(flags & kHasPhysicsMaterial){
            PhysicsMaterialComponent m; std::uint8_t p0=0,p1=0,p2=0,p3=0;
            if(!r.f32(m.friction)||!r.f32(m.restitution)||!r.f32(m.density)||!r.u8(p0)||!r.u8(p1)||!r.u8(p2)||!r.u8(p3)) return setError(r.err);
            world.add(e,m);
        }
        if(flags & kHasPrefab){
            PrefabComponent p; std::uint32_t cnt=0; if(!r.str(p.prefabPath)||!r.u32(cnt)) return setError(r.err);
            for(std::uint32_t k=0;k<cnt;++k){ std::string key,val; if(!r.str(key)||!r.str(val)) return setError(r.err); p.overrides[key]=val; }
            world.add(e,std::move(p));
        }
    }
    if(error) error->clear(); ANA_LOGI("[Scene] loaded %u entities (%zu bytes)", count, data.size()); return true;
}
bool SceneSerializer::saveToFile(const ecs::Registry& world, const std::string& path, std::string* error){
    auto blob=serialize(world); std::FILE* f=std::fopen(path.c_str(),"wb"); if(!f){ if(error) *error="cannot open for writing: "+path; return false; }
    std::size_t written=std::fwrite(blob.data(),1,blob.size(),f); bool ok=(written==blob.size())&&(std::fclose(f)==0); if(!ok&&error) *error="short write: "+path; return ok;
}
bool SceneSerializer::loadFromFile(ecs::Registry& world, const std::string& path, std::string* error){
    std::FILE* f=std::fopen(path.c_str(),"rb"); if(!f){ world.clear(); if(error) *error="cannot open: "+path; return false; }
    std::fseek(f,0,SEEK_END); long sz=std::ftell(f); std::fseek(f,0,SEEK_SET);
    if(sz<0||sz>64L*1024L*1024L){ std::fclose(f); world.clear(); if(error) *error="scene file size invalid"; return false; }
    std::vector<std::uint8_t> data(static_cast<std::size_t>(sz)); std::size_t n=std::fread(data.data(),1,data.size(),f); std::fclose(f);
    if(n!=data.size()){ world.clear(); if(error) *error="short read: "+path; return false; }
    return deserialize(world,data,error);
}
}
