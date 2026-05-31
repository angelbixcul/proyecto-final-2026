#include <iostream>
#include <vector>
#include <fstream>
#include <algorithm>
#include <mutex>
#include "httplib.h"    
#include "json.hpp"     
#include "vehiculo.h"   

using namespace std;
using namespace httplib;

vector<Vehiculo> vehiculos_db;
int proximo_id = 1;
mutex db_mutex; 
const string archivo_datos = "vehiculos.dat";

void guardar_vehiculos_con_lock_adquirido() {
    json j_array = json::array(); 
    for (const auto& v : vehiculos_db) {
        j_array.push_back(v); 
    }

    ofstream archivo(archivo_datos);
    if (archivo.is_open()) {
        archivo << j_array.dump(4);
        archivo.close();
    } else {
        cerr << "Error al abrir " << archivo_datos << " para escritura." << endl;
    }
}

// Función para cargar los vehículos desde el archivo al iniciar el servidor.
void cargar_vehiculos() {
    lock_guard<mutex> lock(db_mutex); // Bloquea el mutex para esta operación

    ifstream archivo(archivo_datos);
    if (archivo.is_open()) {
        try {
            json j_array_leido; 
            archivo >> j_array_leido;
            if (j_array_leido.is_array()) { 
                for (const auto& j_vehiculo : j_array_leido) {
                    // Convierte el objeto JSON a un objeto Vehiculo usando from_json
                    Vehiculo v = j_vehiculo.get<Vehiculo>(); 
                    vehiculos_db.push_back(v);
                    // Actualiza proximo_id para asegurar IDs únicos
                    if (v.id >= proximo_id) {
                        proximo_id = v.id + 1;
                    }
                }
            }
            cout << "Vehículos cargados desde " << archivo_datos << endl;
        } catch (json::parse_error& e) {
            cerr << "Error al parsear JSON desde " << archivo_datos << ": " << e.what() << endl;
        } catch (json::type_error& e) {
            cerr << "Error de tipo JSON desde " << archivo_datos << ": " << e.what() << endl;
        }
        archivo.close();
    } else {
        cout << "No se encontró " << archivo_datos << ". Se iniciará con una base de datos vacía." << endl;
    }
    
    // Asegura que proximo_id sea al menos 1, y mayor que cualquier ID existente.
    if (vehiculos_db.empty() && proximo_id < 1) {
        proximo_id = 1;
    } else if (!vehiculos_db.empty()) {
        int max_id = 0;
        for(const auto& v : vehiculos_db) {
            if (v.id > max_id) max_id = v.id;
        }
        if (proximo_id <= max_id) { 
             proximo_id = max_id + 1;
        }
    }
}


int main(void) {
    Server svr;

    cargar_vehiculos(); 

    // Endpoint POST: Crear un nuevo vehículo
    svr.Post("/vehiculo", [&](const Request &req, Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");

        try {
            json j_body = json::parse(req.body); 
            Vehiculo v_nuevo;
            
            // Validación básica de campos obligatorios
            if (!j_body.contains("modelo") || !j_body.contains("descripcion") || !j_body.contains("color") || !j_body.contains("anio")) {
                 res.status = 400; // Bad Request
                 res.set_content("Faltan campos obligatorios: modelo, descripcion, color, anio", "text/plain; charset=utf-8");
                 return;
            }
            from_json(j_body, v_nuevo); // Usa la función from_json de Vehiculo

            lock_guard<mutex> lock(db_mutex); // Protege el acceso a la BD
            v_nuevo.id = proximo_id++; // Asigna un nuevo ID
            vehiculos_db.push_back(v_nuevo);
            guardar_vehiculos_con_lock_adquirido(); // Guarda los cambios

            json j_respuesta = v_nuevo; // Convierte el nuevo vehículo a JSON para la respuesta
            res.set_content(j_respuesta.dump(4), "application/json; charset=utf-8");
            res.status = 201; // 201 Created
        } catch (json::parse_error& e) {
            res.status = 400;
            res.set_content("JSON mal formado: " + string(e.what()), "text/plain; charset=utf-8");
        } catch (json::type_error& e) {
            res.status = 400;
            res.set_content("Error en el tipo de datos del JSON o campo faltante: " + string(e.what()), "text/plain; charset=utf-8");
        } catch (const exception& e) {
            res.status = 500; // Internal Server Error
            res.set_content("Error interno del servidor: " + string(e.what()), "text/plain; charset=utf-8");
        }
    });

    // Endpoint GET: Obtener todos los vehículos
    svr.Get("/vehiculos", [&](const Request &req, Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");

        lock_guard<mutex> lock(db_mutex);
        json j_array_respuesta = vehiculos_db;
        res.set_content(j_array_respuesta.dump(4), "application/json; charset=utf-8");
    });

    // Endpoint GET: Obtener un vehículo específico por ID
    svr.Get(R"(/vehiculo/(\d+))", [&](const Request &req, Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        // El (\d+) en la ruta es una expresión regular que captura el ID numérico
        int id_vehiculo = stoi(req.matches[1].str()); // req.matches[1] contiene el ID capturado
        
        lock_guard<mutex> lock(db_mutex);
        auto it = find_if(vehiculos_db.begin(), vehiculos_db.end(), 
                               [id_vehiculo](const Vehiculo& v){ return v.id == id_vehiculo; });

        if (it != vehiculos_db.end()) { // Si se encontró el vehículo
            json j_vehiculo_respuesta = *it; 
            res.set_content(j_vehiculo_respuesta.dump(4), "application/json; charset=utf-8");
        } else {
            res.status = 404; // Not Found
            res.set_content("Vehiculo no encontrado", "text/plain; charset=utf-8");
        }
    });

    // Endpoint PUT: Actualizar un vehículo existente por ID
    svr.Put(R"(/vehiculo/(\d+))", [&](const Request &req, Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");

        int id_vehiculo = stoi(req.matches[1].str());
        
        try {
            json j_actualizacion = json::parse(req.body);
            
            lock_guard<mutex> lock(db_mutex);
            auto it = find_if(vehiculos_db.begin(), vehiculos_db.end(), 
                                   [id_vehiculo](const Vehiculo& v){ return v.id == id_vehiculo; });

            if (it != vehiculos_db.end()) {
                // Actualiza solo los campos presentes en el JSON de la petición
                if (j_actualizacion.contains("modelo")) it->modelo = j_actualizacion["modelo"].get<string>();
                if (j_actualizacion.contains("descripcion")) it->descripcion = j_actualizacion["descripcion"].get<string>();
                if (j_actualizacion.contains("color")) it->color = j_actualizacion["color"].get<string>();
                if (j_actualizacion.contains("anio")) it->anio = j_actualizacion["anio"].get<int>();
                
                guardar_vehiculos_con_lock_adquirido();

                json j_respuesta = *it; // Devuelve el vehículo actualizado
                res.set_content(j_respuesta.dump(4), "application/json; charset=utf-8");
            } else {
                res.status = 404;
                res.set_content("Vehiculo no encontrado", "text/plain; charset=utf-8");
            }
        } catch (json::parse_error& e) {
            res.status = 400;
            res.set_content("JSON mal formado: " + string(e.what()), "text/plain; charset=utf-8");
        } catch (json::type_error& e) {
            res.status = 400;
            res.set_content("Error en el tipo de datos del JSON o campo faltante: " + string(e.what()), "text/plain; charset=utf-8");
        } catch (const exception& e) {
            res.status = 500;
            res.set_content("Error interno del servidor: " + string(e.what()), "text/plain; charset=utf-8");
        }
    });

    // Endpoint DELETE: Eliminar un vehículo por ID
    svr.Delete(R"(/vehiculo/(\d+))", [&](const Request& req, Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");

        int id_vehiculo = stoi(req.matches[1].str());

        lock_guard<mutex> lock(db_mutex);
        auto it = find_if(vehiculos_db.begin(), vehiculos_db.end(),
            [id_vehiculo](const Vehiculo& v) { return v.id == id_vehiculo; });

        if (it != vehiculos_db.end()) {
            vehiculos_db.erase(it); // Elimina el vehículo del vector
            guardar_vehiculos_con_lock_adquirido();
            res.status = 204; // No Content (exito, sin cuerpo de respuesta)
        }
        else {
            res.status = 404;
            res.set_content("Vehiculo no encontrado", "text/plain; charset=utf-8");
        }
    });
    
    svr.Options(R"(.*)", [](const Request& req, Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 204; // No Content
        });
    
    cout << "Servidor CRUD de vehiculos iniciando en http://localhost:8080" << endl;
    svr.listen("0.0.0.0", 8080); // Inicia el servidor para escuchar en el puerto 8080 en todas las interfaces

    return 0;
}
