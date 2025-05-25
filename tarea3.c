#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "extra.h"
#include "list.h"
#include "map.h"

#define MAX_NAME 50
#define MAX_DESC 200
#define INITIAL_TIME 10

typedef struct {
    char nombre[MAX_NAME];
    int valor;
    int peso;
} Item;

typedef struct {
    int id;
    char nombre[MAX_NAME];
    char descripcion[MAX_DESC];
    List* items;               
    int conexiones[4];//Arriba, Abajo, Izquierda, Derecha
    int is_final;
} Escenario;

typedef struct {
    Map* nodos;//Map para el grafo
} Grafo;

typedef struct {
    Escenario* current;//Escenario actual
    List* inventario; //Lista de items en el inventario
    int peso_total;//Peso total del inventario
    int puntaje;//Puntaje del jugador
    int tiempo_restante;
} EstadoJuego;

Grafo* grafo = NULL; //Grafo global
EstadoJuego game;   //Estado del juego global

//Función de comparación para Map
int int_equal(void* a, void* b) 
{
    return (*(int*)a == *(int*)b);
}

// Elimina espacios en blanco al inicio y final de una cadena.
void eliminar_espacios(char* str) 
{
    char* end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    *(end+1) = 0;
}

// Limpia el buffer de entrada estándar (stdin) para evitar errores de lectura.
void limpiar_buffer() 
{
    while(getchar() != '\n');
}

// Carga el laberinto desde un archivo CSV y lo almacena en un grafo usando Map.
// Lee escenarios, items y conexiones, y los guarda en memoria dinámica.
void cargar_laberinto() {
    FILE* file = fopen("graphquest.csv", "r");
    if(!file) {
        printf("Error al abrir el archivo graphquest.csv\n");
        exit(1);
    }

    grafo = malloc(sizeof(Grafo));
    grafo->nodos = map_create(int_equal);

    char** campos = leer_linea_csv(file, ','); // Saltar encabezado
    // Leer cada línea del archivo CSV
    while((campos = leer_linea_csv(file, ',')) != NULL) {
        Escenario* s = malloc(sizeof(Escenario));
        s->id = atoi(campos[0]);
        strncpy(s->nombre, campos[1], MAX_NAME - 1);
        s->nombre[MAX_NAME - 1] = '\0';
        eliminar_espacios(s->nombre);
        
        strncpy(s->descripcion, campos[2], MAX_DESC - 1);
        s->descripcion[MAX_DESC - 1] = '\0';
        eliminar_espacios(s->descripcion);

        //Procesar items usando split_string
        s->items = list_create();
        if(strlen(campos[3]) > 0 && strcmp(campos[3], "-") != 0) {
            List* items_raw = split_string(campos[3], ";");
            for(char* item_str = list_first(items_raw); item_str != NULL; item_str = list_next(items_raw)) {
                List* valores = split_string(item_str, ",");
                if(list_size(valores) >= 3) {
                    Item* it = malloc(sizeof(Item)); //abreviacion de item
                    strncpy(it->nombre, list_first(valores), MAX_NAME - 1);
                    it->nombre[MAX_NAME - 1] = '\0';
                    eliminar_espacios(it->nombre);
                    it->valor = atoi(list_next(valores));
                    it->peso = atoi(list_next(valores));
                    list_pushBack(s->items, it);
                }
                list_clean(valores);
                free(valores);
            }
            list_clean(items_raw);
            free(items_raw);
        }

        // Conexiones 
        for(int j = 0; j < 4; j++) 
        {
            s->conexiones[j] = atoi(campos[4 + j]);
        }

        // Es final
        s->is_final = (strcmp(campos[8], "Si") == 0) ? 1 : 0;

        //Insertar en Map
        int* key = malloc(sizeof(int));
        *key = s->id;
        map_insert(grafo->nodos, key, s);
    }

    fclose(file);
}

// Inicializa el estado del juego: busca el escenario inicial, 
// resetea inventario, peso, puntaje y tiempo.
void iniciar_juego() 
{
    //Buscar escenario inicial usando Map
    int id_inicial = 1;
    MapPair* mp = map_search(grafo->nodos, &id_inicial);
    if(!mp) {
        printf("Error: No se encontro el escenario inicial\n");
        exit(1);
    }
    game.current = (Escenario*)mp->value;
    game.inventario = list_create();       //Inicializar List
    game.peso_total = 0;
    game.puntaje = 0;
    game.tiempo_restante = INITIAL_TIME;
}

// Muestra en pantalla el estado actual del jugador: 
// escenario, items, inventario, conexiones y tiempo.
void mostrar_estado_actual() 
{
    printf("\n=== %s ===\n", game.current->nombre);
    printf("%s\n", game.current->descripcion);
    
    printf("\nItems disponibles:\n");
    int idx = 1;
    //REVISAR  Y CAMBIAR
    for(Item* it = list_first(game.current->items); it != NULL; it = list_next(game.current->items), idx++) 
    {
        printf("%d. %s (Valor: %d, Peso: %d)\n", 
               idx, it->nombre, it->valor, it->peso);
    }
    if(idx == 1) printf("No hay items en este escenario\n");
    
    printf("\nInventario (Peso: %d, Puntos: %d):\n", game.peso_total, game.puntaje);
    idx = 1;
    for(Item* it = list_first(game.inventario); it != NULL; it = list_next(game.inventario), idx++) 
    {
        printf("%d. %s (Peso: %d, Valor: %d)\n", 
               idx, it->nombre, it->peso, it->valor);
    }
    if(idx == 1) printf("Inventario vacio\n");
    
    printf("\nTiempo restante: %d\n", game.tiempo_restante);
    
    printf("\nConexiones disponibles:\n");
    if(game.current->conexiones[0] != -1) printf("1. Arriba (Escenario %d)\n", game.current->conexiones[0]);
    if(game.current->conexiones[1] != -1) printf("2. Abajo (Escenario %d)\n", game.current->conexiones[1]);
    if(game.current->conexiones[2] != -1) printf("3. Izquierda (Escenario %d)\n", game.current->conexiones[2]);
    if(game.current->conexiones[3] != -1) printf("4. Derecha (Escenario %d)\n", game.current->conexiones[3]);
}

// Permite al jugador recoger un item del escenario actual.
// Suma su valor y peso al inventario y lo elimina del escenario.
void tomar_item() 
{
    int contador_items = list_size(game.current->items); //Contar items en el escenario
    if(contador_items == 0) 
    {
        printf("No hay items para recoger en este escenario.\n");
        return;
    }
    
    printf("Seleccione el item a recoger (1-%d) o 0 para cancelar: ", contador_items);
    int opcion;
    scanf("%d", &opcion);
    limpiar_buffer();
    
    if(opcion < 1 || opcion > contador_items) 
    {
        printf("Seleccion invalida.\n");
        return;
    }
    
    // Buscar item por índice
    int idx = 1;
    Item* tomado = NULL;
    for(Item* it = list_first(game.current->items); it != NULL; it = list_next(game.current->items), idx++) 
    {
        if(idx == opcion) 
        {
            tomado = it;
            break;
        }
    }
    
    if(!tomado) 
    {
        printf("Error al seleccionar item.\n");
        return;
    }
    
    //Agregar al inventario
    Item* new_item = malloc(sizeof(Item));
    *new_item = *tomado; // Copiar item
    list_pushBack(game.inventario, new_item);
    game.puntaje += tomado->valor;
    game.peso_total += tomado->peso;
    
    //Eliminar del escenario
    list_first(game.current->items);
    for(int i = 1; i < opcion; i++) 
    {
        list_next(game.current->items);
    }
    list_popCurrent(game.current->items);
    free(tomado);
    
    game.tiempo_restante--;
    printf("Has recogido: %s\n", new_item->nombre);
}

// Permite al jugador descartar un item del inventario.
// Resta su valor y peso, y lo elimina del inventario.
void descartar_item() {
    int contador_inventario = list_size(game.inventario); //contador objetos en el inventario
    if(contador_inventario == 0) 
    {
        printf("No hay items en tu inventario.\n");
        return;
    }
    
    printf("Seleccione el item a descartar (1-%d) o 0 para cancelar: ", contador_inventario);
    int opcion;
    scanf("%d", &opcion);
    limpiar_buffer();
    
    if(opcion < 1 || opcion > contador_inventario) 
    {
        printf("Seleccion invalida.\n");
        return;
    }
    
    //Buscar item por índice
    int idx = 1;
    Item* descartado = NULL;
    for(Item* it = list_first(game.inventario); it != NULL; it = list_next(game.inventario), idx++) 
    {
        if(idx == opcion) {
            descartado = it;
            break;
        }
    }
    
    if(!descartado) 
    {
        printf("Error al seleccionar item.\n");
        return;
    }
    
    game.puntaje -= descartado->valor;
    game.peso_total -= descartado->peso;
    
    printf("Has descartado: %s\n", descartado->nombre);
    
    //Eliminar del inventario
    list_first(game.inventario);
    for(int i = 1; i < opcion; i++)
    {
        list_next(game.inventario);
    }
    list_popCurrent(game.inventario);
    free(descartado);
    
    game.tiempo_restante--;
}

// Permite al jugador moverse a otro escenario según las conexiones.
// Calcula el costo de tiempo en función del peso total.
void moverse() {
    printf("Seleccione direccion:\n");
    if(game.current->conexiones[0] != -1) printf("1. Arriba\n");
    if(game.current->conexiones[1] != -1) printf("2. Abajo\n");
    if(game.current->conexiones[2] != -1) printf("3. Izquierda\n");
    if(game.current->conexiones[3] != -1) printf("4. Derecha\n");
    printf("0. Cancelar\n");
    
    int opcion;
    scanf("%d", &opcion);
    limpiar_buffer();
    
    if(opcion < 1 || opcion > 4)
    {
        printf("Movimiento cancelado.\n");
        return;
    }
    
    int direccion = opcion - 1;
    if(game.current->conexiones[direccion] == -1) 
    {
        printf("Direccion no valida.\n");
        return;
    }
    
    // Calcular tiempo consumido
    int tiempo_consumido = (int)ceil((game.peso_total + 1) / 10.0);
    if(tiempo_consumido < 1) tiempo_consumido = 1;
    
    if(game.tiempo_restante < tiempo_consumido) 
    {
        printf("No tienes suficiente tiempo para moverserte con tanto peso!\n");
        return;
    }
    
    // Buscar escenario usando Map
    int nueva_id_escenario = game.current->conexiones[direccion];
    MapPair* mp = map_search(grafo->nodos, &nueva_id_escenario);
    if(!mp)
    {
        printf("Error: Escenario no encontrado.\n");
        return;
    }
    
    game.current = (Escenario*)mp->value;
    game.tiempo_restante -= tiempo_consumido;
    
    printf("Te has movido a: %s\n", game.current->nombre);
}

void menu()
{
    printf("\n=== MENU ===\n");
    printf("1. Recoger item\n");
    printf("2. Descartar item\n");
    printf("3. Moverse\n");
    printf("4. Reiniciar juego\n");
    printf("5. Salir\n");
    printf("Seleccione una opcion: ");
}
//Esta funcion reinicia el juego, limpiando el inventario y volviendo al punto inicial
void resetear() 
{
    // Limpiar inventario
    for(Item* it = list_first(game.inventario); it != NULL; it = list_next(game.inventario)) {
        free(it);
    }
    list_clean(game.inventario);
    
    iniciar_juego();
    printf("Juego reiniciado.\n");
}

// Libera toda la memoria dinámica utilizada por el juego: 
// items, escenarios, inventario y grafo.
void cleanup() 
{
    if(grafo) 
    {
        // Liberar cada escenario y sus items
        for(MapPair* pair = map_first(grafo->nodos); pair != NULL; pair = map_next(grafo->nodos))
        {
            Escenario* s = (Escenario*)pair->value;
            for(Item* it = list_first(s->items); it != NULL; it = list_next(s->items))
            {
                free(it);
            }
            list_clean(s->items);
            free(s->items);
            free(pair->key);
            free(s);
        }
        map_clean(grafo->nodos);
        free(grafo->nodos);
        free(grafo);
    }
    
    // Limpiar inventario
    if(game.inventario)
    {
        for(Item* it = list_first(game.inventario); it != NULL; it = list_next(game.inventario))
        {
            free(it);
        }
        list_clean(game.inventario);
        free(game.inventario);
    }
}

// Función principal: ejecuta el bucle principal del juego, 
// gestiona las opciones del menú y controla el flujo general.
int main() 
{
    cargar_laberinto();
    iniciar_juego();
    
    printf("=== BIENVENIDO A GRAPHQUEST ===\n");
    
    int game_over = 0; //controla si el juego ha terminado
    while(!game_over && game.tiempo_restante > 0)
    {
        mostrar_estado_actual();
        
        if(game.current->is_final)
        {
            printf("\n¡Felicidades! Has llegado al escenario final.\n");
            printf("Puntaje final: %d\n", game.puntaje);
            break;
        }
        
        menu();
        
        int opcion;
        scanf("%d", &opcion);
        limpiar_buffer();
        
        switch(opcion) {
            case 1:
                tomar_item();
                break;
            case 2:
                descartar_item();
                break;
            case 3:
                moverse();
                break;
            case 4:
                resetear();
                break;
            case 5:
                game_over = 1;
                break;
            default:
                printf("Opcion invalida.\n");
        }
        
        if(game.tiempo_restante <= 0) {
            printf("\n¡Se te ha acabado el tiempo!\n");
            printf("Puntaje final: %d\n", game.puntaje);
        }
    }
    
    cleanup();
    return 0;
}
