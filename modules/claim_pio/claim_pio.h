/**
 * Copyright (c) 2026 metalu.net
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _CLAIM_PIO_H
#define _CLAIM_PIO_H


/** \file claim_pio.h
 *  \defgroup claim_pio claim_pio
 * Common Fraise API
 *  \ingroup pico
 *
 */
///@{

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Request attribution of a pio/sm (and optionnaly irq) for a given pio program
 * \param program the pio program we want a sm to run
 * \param pio_hw the address where to store the available pio
 * \param sm the address where to store the index of the available state machine
 * \param program_offset the address where to store the offset from which the program can be written
 * \param irq if not null, the address where to store the index of the chosen irq
 * \return false if this fails.
 */
bool claim_pio_sm_irq(const pio_program_t *program, PIO *pio_hw, uint *sm, uint *program_offset, uint *irq);

#ifdef __cplusplus
}
#endif

///@}

#endif // _CLAIM_PIO_H
