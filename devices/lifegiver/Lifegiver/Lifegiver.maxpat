{
	"patcher": {
		"fileversion": 1,
		"appversion": {
			"major": 8,
			"minor": 6,
			"revision": 4,
			"architecture": "x64",
			"modernui": 1
		},
		"classnamespace": "box",
		"rect": [
			60.0,
			60.0,
			1400.0,
			900.0
		],
		"openrect": [
			0.0,
			0.0,
			144.0,
			169.0
		],
		"bglocked": 0,
		"openinpresentation": 1,
		"default_fontsize": 10.0,
		"default_fontface": 0,
		"default_fontname": "Arial Bold",
		"gridonopen": 1,
		"gridsize": [
			8.0,
			8.0
		],
		"gridsnaponopen": 1,
		"objectsnaponopen": 1,
		"statusbarvisible": 2,
		"toolbarvisible": 1,
		"boxanimatetime": 200,
		"enablehscroll": 1,
		"enablevscroll": 1,
		"devicewidth": 144.0,
		"description": "Lifegiver",
		"digest": "",
		"tags": "",
		"style": "",
		"subpatcher_template": "",
		"boxes": [
			{
				"box": {
					"id": "obj-13",
					"maxclass": "panel",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						970.0,
						20.0,
						40.0,
						30.0
					],
					"presentation": 1,
					"presentation_rect": [
						6.0,
						4.0,
						132.0,
						161.0
					],
					"bgcolor": [
						0.11,
						0.114,
						0.125,
						1.0
					],
					"border": 0,
					"rounded": 6,
					"background": 1,
					"ignoreclick": 1,
					"mode": 0
				}
			},
			{
				"box": {
					"id": "obj-1",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 2,
					"patching_rect": [
						40.0,
						260.0,
						60.0,
						22.0
					],
					"outlettype": [
						"signal",
						"signal"
					],
					"text": "plugin~"
				}
			},
			{
				"box": {
					"id": "obj-2",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 3,
					"patching_rect": [
						40.0,
						300.0,
						140.0,
						22.0
					],
					"outlettype": [
						"signal",
						"signal",
						""
					],
					"text": "lifegiver~ ---lfg"
				}
			},
			{
				"box": {
					"id": "obj-3",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 0,
					"patching_rect": [
						40.0,
						350.0,
						60.0,
						22.0
					],
					"text": "plugout~"
				}
			},
			{
				"box": {
					"id": "obj-4",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 3,
					"patching_rect": [
						40.0,
						20.0,
						90.0,
						22.0
					],
					"outlettype": [
						"bang",
						"int",
						"int"
					],
					"text": "live.thisdevice"
				}
			},
			{
				"box": {
					"id": "obj-5",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 3,
					"patching_rect": [
						40.0,
						50.0,
						60.0,
						22.0
					],
					"outlettype": [
						"bang",
						"bang",
						"bang"
					],
					"text": "t b b b"
				}
			},
			{
				"box": {
					"id": "obj-6",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						40.0,
						80.0,
						40.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "init"
				}
			},
			{
				"box": {
					"id": "obj-7",
					"maxclass": "message",
					"numinlets": 2,
					"numoutlets": 1,
					"patching_rect": [
						140.0,
						80.0,
						110.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "patch 2026-10-15a"
				}
			},
			{
				"box": {
					"id": "obj-8",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						140.0,
						110.0,
						90.0,
						22.0
					],
					"text": "print lifegiver"
				}
			},
			{
				"box": {
					"id": "obj-9",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 3,
					"patching_rect": [
						600.0,
						300.0,
						82.0,
						22.0
					],
					"outlettype": [
						"",
						"",
						""
					],
					"text": "pattr state",
					"parameter_enable": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "State",
							"parameter_shortname": "State",
							"parameter_type": 3,
							"parameter_mmin": 0,
							"parameter_mmax": 1,
							"parameter_initial": [
								0
							],
							"parameter_initial_enable": 1,
							"parameter_unitstyle": 1,
							"parameter_invisible": 1
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-10",
					"maxclass": "newobj",
					"numinlets": 2,
					"numoutlets": 3,
					"patching_rect": [
						200.0,
						400.0,
						160.0,
						22.0
					],
					"outlettype": [
						"",
						"",
						""
					],
					"text": "route getstate dictionary"
				}
			},
			{
				"box": {
					"id": "obj-11",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						200.0,
						430.0,
						30.0,
						22.0
					],
					"outlettype": [
						"bang"
					],
					"text": "t b"
				}
			},
			{
				"box": {
					"id": "obj-12",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						260.0,
						460.0,
						110.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend dictionary"
				}
			},
			{
				"box": {
					"id": "obj-14",
					"maxclass": "live.comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						1000.0,
						53.0,
						80.0,
						18.0
					],
					"text": "LIFEGIVER",
					"presentation": 1,
					"presentation_rect": [
						9.0,
						5.0,
						126.0,
						14.0
					],
					"textcolor": [
						0.941,
						0.541,
						0.478,
						1.0
					],
					"fontname": "Ableton Sans Bold",
					"fontsize": 9.5,
					"fontface": 1
				}
			},
			{
				"box": {
					"id": "obj-15",
					"maxclass": "panel",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						1100.0,
						54.0,
						80.0,
						2.0
					],
					"presentation": 1,
					"presentation_rect": [
						12.0,
						19.0,
						120.0,
						2.0
					],
					"bgcolor": [
						0.941,
						0.541,
						0.478,
						0.55
					],
					"border": 0,
					"rounded": 1,
					"ignoreclick": 1
				}
			},
			{
				"box": {
					"id": "obj-16",
					"maxclass": "live.comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						1200.0,
						55.0,
						60.0,
						18.0
					],
					"text": "first build: pass-through",
					"presentation": 1,
					"presentation_rect": [
						10.0,
						26.0,
						124.0,
						13.0
					],
					"textcolor": [
						0.55,
						0.55,
						0.56,
						1.0
					],
					"fontsize": 8.0,
					"textjustification": 1
				}
			},
			{
				"box": {
					"id": "obj-17",
					"maxclass": "live.dial",
					"numinlets": 1,
					"numoutlets": 2,
					"patching_rect": [
						700.0,
						600.0,
						44.0,
						48.0
					],
					"outlettype": [
						"",
						"float"
					],
					"presentation": 1,
					"presentation_rect": [
						50.0,
						52.0,
						44.0,
						50.0
					],
					"varname": "Amount",
					"parameter_enable": 1,
					"annotation": "How much of Lifegiver you hear: 0 % = the original, untouched; 100 % = the full plan. In this first build there is no plan yet, so the sound passes through unchanged.",
					"annotation_name": "Amount",
					"fontsize": 8.5,
					"activedialcolor": [
						0.941,
						0.541,
						0.478,
						1.0
					],
					"activeneedlecolor": [
						0.941,
						0.541,
						0.478,
						1.0
					],
					"dialcolor": [
						0.055,
						0.055,
						0.063,
						1.0
					],
					"textcolor": [
						0.925,
						0.925,
						0.925,
						1.0
					],
					"showname": 1,
					"shownumber": 1,
					"saved_attribute_attributes": {
						"valueof": {
							"parameter_longname": "Amount",
							"parameter_shortname": "Amount",
							"parameter_type": 0,
							"parameter_mmin": 0,
							"parameter_mmax": 100,
							"parameter_initial": [
								100
							],
							"parameter_initial_enable": 1,
							"parameter_unitstyle": 5
						}
					}
				}
			},
			{
				"box": {
					"id": "obj-18",
					"maxclass": "newobj",
					"numinlets": 1,
					"numoutlets": 1,
					"patching_rect": [
						700.0,
						655.0,
						100.0,
						22.0
					],
					"outlettype": [
						""
					],
					"text": "prepend amount"
				}
			},
			{
				"box": {
					"id": "obj-19",
					"maxclass": "live.comment",
					"numinlets": 1,
					"numoutlets": 0,
					"patching_rect": [
						1200.0,
						58.0,
						60.0,
						18.0
					],
					"text": "build 2026-10-15a",
					"presentation": 1,
					"presentation_rect": [
						10.0,
						147.0,
						124.0,
						13.0
					],
					"textcolor": [
						0.55,
						0.55,
						0.56,
						1.0
					],
					"fontsize": 7.5,
					"textjustification": 1
				}
			}
		],
		"lines": [
			{
				"patchline": {
					"source": [
						"obj-1",
						0
					],
					"destination": [
						"obj-2",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-1",
						1
					],
					"destination": [
						"obj-2",
						1
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-2",
						0
					],
					"destination": [
						"obj-3",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-2",
						1
					],
					"destination": [
						"obj-3",
						1
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-4",
						0
					],
					"destination": [
						"obj-5",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-5",
						0
					],
					"destination": [
						"obj-6",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-6",
						0
					],
					"destination": [
						"obj-2",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-5",
						1
					],
					"destination": [
						"obj-7",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-7",
						0
					],
					"destination": [
						"obj-8",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-9",
						0
					],
					"destination": [
						"obj-2",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-2",
						2
					],
					"destination": [
						"obj-10",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-10",
						0
					],
					"destination": [
						"obj-11",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-11",
						0
					],
					"destination": [
						"obj-9",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-10",
						1
					],
					"destination": [
						"obj-12",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-12",
						0
					],
					"destination": [
						"obj-9",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-17",
						0
					],
					"destination": [
						"obj-18",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-18",
						0
					],
					"destination": [
						"obj-2",
						0
					]
				}
			},
			{
				"patchline": {
					"source": [
						"obj-5",
						2
					],
					"destination": [
						"obj-17",
						0
					]
				}
			}
		],
		"dependency_cache": [
			{
				"name": "lifegiver~.mxo",
				"type": "iLaX"
			}
		],
		"latency": 0,
		"is_mpe": 0,
		"minimum_live_version": "",
		"minimum_max_version": "",
		"platform_compatibility": 0,
		"project": {
			"version": 1,
			"creationdate": 3590052493,
			"modificationdate": 3590052493,
			"viewrect": [
				0.0,
				0.0,
				300.0,
				500.0
			],
			"autoorganize": 1,
			"hideprojectwindow": 1,
			"showdependencies": 1,
			"autolocalize": 0,
			"contents": {
				"patchers": {}
			},
			"layout": {},
			"searchpath": {},
			"detailsvisible": 0,
			"amxdtype": 1633771873,
			"readonly": 0,
			"devpathtype": 0,
			"devpath": ".",
			"sortmode": 0,
			"viewmode": 0,
			"includepackages": 0
		},
		"autosave": 0
	}
}