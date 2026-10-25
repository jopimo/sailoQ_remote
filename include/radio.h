#ifndef RADIO_H
#define RADIO_H
#include <stdint.h>
#include "sq_proto.h"
#include "sq_mando.h"           /* libreria sailoQ_radio, lado del mando */

/* Estado de cada receptor visto desde el mando */
typedef sq_link_t peer_state_t;

extern pkt_nav_t        nav;        /* ultimos datos de la base */
extern pkt_act_status_t act;        /* ultimo estado del actuador */
extern peer_state_t     st_base, st_act;

int  radio_init(void);              /* 0 = modulo presente */
int  radio_ok(void);
uint32_t radio_remote_id(void);

void radio_wake(void);              /* sale de ahorro (antes de una rafaga de paquetes) */
void radio_sleep(void);

int  radio_poll_base(uint8_t page, uint32_t now);         /* pide datos; 0 = OK */
int  radio_pilot_key(uint8_t key, uint32_t now);          /* manda una tecla al piloto; 0 = OK (con acuse) */
int  radio_act_hold(uint8_t func, uint32_t now);          /* mantiene una funcion del actuador (repetir cada 100 ms) */
int  radio_act_poll(uint32_t now);

/* Emparejamiento: se llama a pair_step() cada ~100 ms mientras dure (maximo 60 s).
 * Devuelve 0 = sigue buscando, 1 = emparejado (cfg ya guardada), -1 = error. */
void radio_pair_start(uint8_t dev_type);
int  radio_pair_step(void);

/* 1 si hubo acuse hace menos de 'ms' */
int  radio_alive(const peer_state_t *p, uint32_t now, uint32_t ms);

#endif
