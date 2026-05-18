#pragma once

/**
 * @file carousel.h
 * @brief Carrusel de las 23 vistas de la UI — GrooveForge Brain
 *
 * Modo demo sin hardware de entrada: muestra cada vista 5s y avanza sola, en
 * bucle infinito, con transicion radial-wipe entre vistas.
 *
 * Ver apps/docs/sprints/18-display-ui-carousel.md.
 */

/**
 * @brief Arranca el carrusel. Llamar una sola vez, despues de la animacion
 *        de boot. Construye la vista 01 y programa el avance automatico.
 */
void carousel_start(void);
