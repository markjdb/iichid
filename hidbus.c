/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Vladimir Kondratyev <wulf@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/systm.h>

#include "hidbus.h"

#include "hid_if.h"

static hid_intr_t	hidbus_intr;

struct hidbus_softc {
	device_t	dev;
	struct mtx	lock;

	device_t	child;
	hid_intr_t	*intr;
};

static int
hidbus_probe(device_t dev)
{

	device_set_desc(dev, "HID bus");

	/* Allow other subclasses to override this driver. */
	return (BUS_PROBE_GENERIC);
}

static int
hidbus_attach(device_t dev)
{
	struct hidbus_softc *sc = device_get_softc(dev);
	device_t child;
	int error;
	void *ivars = device_get_ivars(dev);

	sc->dev = dev;

	child = device_add_child(dev, NULL, -1);
	if (child == NULL) {
		device_printf(dev, "Could not add HID device\n");
		return (ENXIO);
	}

	mtx_init(&sc->lock, "hidbus lock", NULL, MTX_DEF);
	HID_INTR_SETUP(device_get_parent(dev), &sc->lock, hidbus_intr, sc);
	device_set_ivars(child, ivars);
	error = bus_generic_attach(dev);
	if (error)
		device_printf(dev, "failed to attach child: error %d\n", error);

	return (0);
}

static int
hidbus_detach(device_t dev)
{
	struct hidbus_softc *sc = device_get_softc(dev);

	bus_generic_detach(dev);
	device_delete_children(dev);

	HID_INTR_UNSETUP(device_get_parent(dev));
	mtx_destroy(&sc->lock);

	return (0);
}

struct mtx *
hid_get_lock(device_t child)
{
	struct hidbus_softc *sc = device_get_softc(device_get_parent(child));

	return (&sc->lock);
}

void
hid_set_intr(device_t child, hid_intr_t intr)
{
	device_t bus = device_get_parent(child);
	struct hidbus_softc *sc = device_get_softc(bus);

	sc->child = child;
	sc->intr = intr;
}

void
hidbus_intr(void *context, void *buf, uint16_t len)
{
	struct hidbus_softc *sc = context;

	sc->intr(sc->child, buf, len);
}

int
hid_start(device_t child)
{
	device_t bus = device_get_parent(child);

	return (HID_INTR_START(device_get_parent(bus)));
}

int
hid_stop(device_t child)
{
	device_t bus = device_get_parent(child);

	return (HID_INTR_STOP(device_get_parent(bus)));
}

/*
 * HID interface
 */
int
hid_get_report_descr(device_t bus, void **data, uint16_t *len)
{

	return (HID_GET_REPORT_DESCR(device_get_parent(bus), data, len));
}

int
hid_get_input_report(device_t bus, void *data, uint16_t len)
{

	return (HID_GET_INPUT_REPORT(device_get_parent(bus), data, len));
}

int
hid_set_output_report(device_t bus, void *data, uint16_t len)
{

	return (HID_SET_OUTPUT_REPORT(device_get_parent(bus), data, len));
}

int
hid_get_report(device_t bus, void *data, uint16_t len, uint8_t type,
    uint8_t id)
{

	return (HID_GET_REPORT(device_get_parent(bus), data, len, type, id));
}

int
hid_set_report(device_t bus, void *data, uint16_t len, uint8_t type,
    uint8_t id)
{

	return (HID_SET_REPORT(device_get_parent(bus), data, len, type, id));
}

int
hid_set_idle(device_t bus, uint16_t duration, uint8_t id)
{

	return (HID_SET_IDLE(device_get_parent(bus), duration, id));
}

int
hid_set_protocol(device_t bus, uint16_t protocol)
{

	return (HID_SET_PROTOCOL(device_get_parent(bus), protocol));
}

static device_method_t hidbus_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,         hidbus_probe),
	DEVMETHOD(device_attach,        hidbus_attach),
	DEVMETHOD(device_detach,        hidbus_detach),
	DEVMETHOD(device_suspend,       bus_generic_suspend),
	DEVMETHOD(device_resume,        bus_generic_resume),

	/* hid interface */
	DEVMETHOD(hid_get_report_descr,	hid_get_report_descr),
	DEVMETHOD(hid_get_input_report,	hid_get_input_report),
	DEVMETHOD(hid_set_output_report,hid_set_output_report),
	DEVMETHOD(hid_get_report,       hid_get_report),
	DEVMETHOD(hid_set_report,       hid_set_report),
	DEVMETHOD(hid_set_idle,		hid_set_idle),
	DEVMETHOD(hid_set_protocol,	hid_set_protocol),

        DEVMETHOD_END
};


driver_t hidbus_driver = {
	"hidbus",
	hidbus_methods,
	sizeof(struct hidbus_softc),
};

devclass_t hidbus_devclass;

MODULE_VERSION(hidbus, 1);
DRIVER_MODULE(hidbus, usbhid, hidbus_driver, hidbus_devclass, 0, 0);
DRIVER_MODULE(hidbus, iichid, hidbus_driver, hidbus_devclass, 0, 0);
