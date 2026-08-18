#!/usr/bin/env python


def can_build(env, platform):
    return True


def configure(env):
    pass


def get_doc_classes():
    return [
        # SimServer (S-01..S-05) — systemic / immersive-sim server
        "SimServer",
        "SurfaceProperties",
        # Combat subsystem (C-14, moved from modules/combat/)
        "Hitbox3D",
        "Hurtbox3D",
        "Projectile3D",
    ]


def get_doc_path():
    return "doc_classes"


def get_icons_path():
    return "editor/icons"
