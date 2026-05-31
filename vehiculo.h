#ifndef VEHICULO_H 
#define VEHICULO_H
#include <string>
#include "json.hpp" 

using namespace std;
using json = nlohmann::ordered_json;

struct Vehiculo {
    int id;
    string modelo;
    string descripcion;
    string color;
    int anio;

    // Amigas para la serialización/deserialización con nlohmann/json
    friend void to_json(json& j, const Vehiculo& v) {
        j = json{
            {"id", v.id},
            {"modelo", v.modelo},
            {"descripcion", v.descripcion},
            {"color", v.color},
            {"anio", v.anio}
        };
    }

    friend void from_json(const json& j, Vehiculo& v) {
        // El ID se asigna al crear o se lee del archivo,
        // por lo que solo lo leemos si está presente (útil para cargar desde archivo)
        if (j.contains("id")) {
            j.at("id").get_to(v.id);
        }

        // Para los campos que vienen del cliente, es bueno verificar si existen
        // antes de intentar accederlos para evitar excepciones si faltan.
        if (j.contains("modelo")) j.at("modelo").get_to(v.modelo);
        if (j.contains("descripcion")) j.at("descripcion").get_to(v.descripcion);
        if (j.contains("color")) j.at("color").get_to(v.color);
        if (j.contains("anio")) j.at("anio").get_to(v.anio);
    }
};

#endif 
#pragma once
