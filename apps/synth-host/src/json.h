#pragma once
#include <json-c/json.h>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>
namespace host {
/** @brief JSON ownership and strict field validation, used only by control. */
class Json {
    json_object* p_ = nullptr;
    explicit Json(json_object* p, bool retain): p_(p) { if(retain && p_) json_object_get(p_); }
public:
    Json():p_(json_object_new_object()) {}
    Json(std::nullptr_t) {}
    Json(bool x):p_(json_object_new_boolean(x)) {}
    Json(int x):p_(json_object_new_int(x)) {}
    Json(int64_t x):p_(json_object_new_int64(x)) {}
    Json(double x):p_(json_object_new_double(x)) {}
    Json(float x):Json(double(x)) {}
    Json(const char* x):p_(json_object_new_string(x)) {}
    Json(const std::string& x):p_(json_object_new_string_len(x.data(), int(x.size()))) {}
    Json(const Json& x):Json(x.p_,true) {}
    Json(Json&& x) noexcept:p_(std::exchange(x.p_,nullptr)) {}
    Json& operator=(Json x) { std::swap(p_,x.p_); return *this; }
    ~Json() { if(p_) json_object_put(p_); }
    static Json array() { return Json(json_object_new_array(),false); }
    static Json parse(const std::string& s) {
        auto* t=json_tokener_new_ex(32);
        json_tokener_set_flags(t,JSON_TOKENER_STRICT | JSON_TOKENER_VALIDATE_UTF8);
        auto* p=json_tokener_parse_ex(t,s.c_str(),int(s.size()+1));
        const bool ok=json_tokener_get_error(t)==json_tokener_success;
        json_tokener_free(t);
        if(!ok) { if(p) json_object_put(p); throw std::runtime_error("invalid JSON"); }
        return Json(p,false);
    }
    std::string dump() const { return json_object_to_json_string_ext(p_,JSON_C_TO_STRING_PLAIN); }
    bool is(json_type t) const { return json_object_get_type(p_)==t; }
    bool has(const std::string& k) const { json_object* x; return is(json_type_object)&&json_object_object_get_ex(p_,k.c_str(),&x); }
    Json at(const std::string& k) const { json_object* x; if(!is(json_type_object)||!json_object_object_get_ex(p_,k.c_str(),&x)) throw std::runtime_error("missing field: "+k); return Json(x,true); }
    Json at(size_t i) const { if(!is(json_type_array)||i>=size()) throw std::runtime_error("invalid array index"); return Json(json_object_array_get_idx(p_,i),true); }
    size_t size() const { if(!is(json_type_array)) throw std::runtime_error("expected array"); return json_object_array_length(p_); }
    std::string str() const { if(!is(json_type_string)) throw std::runtime_error("expected string"); return {json_object_get_string(p_),size_t(json_object_get_string_len(p_))}; }
    double number() const { if(!is(json_type_double)&&!is(json_type_int)) throw std::runtime_error("expected number"); double x=json_object_get_double(p_); if(!std::isfinite(x)) throw std::runtime_error("expected finite number"); return x; }
    int64_t integer() const { if(!is(json_type_int)) throw std::runtime_error("expected integer"); return json_object_get_int64(p_); }
    bool boolean() const { if(!is(json_type_boolean)) throw std::runtime_error("expected boolean"); return json_object_get_boolean(p_); }
    void set(const std::string& k,Json v) { if(!is(json_type_object)) throw std::runtime_error("expected object"); json_object_object_add(p_,k.c_str(),json_object_get(v.p_)); }
    void push(Json v) { if(!is(json_type_array)) throw std::runtime_error("expected array"); json_object_array_add(p_,json_object_get(v.p_)); }
    size_t fields() const { if(!is(json_type_object)) throw std::runtime_error("expected object"); return json_object_object_length(p_); }
};
inline Json object(std::initializer_list<std::pair<std::string,Json>> fields) { Json j; for(const auto& f:fields) j.set(f.first,f.second); return j; }
}
