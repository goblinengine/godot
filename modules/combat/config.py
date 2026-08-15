#!/usr/bin/env python

def can_build(env, platform):
    return True


def configure(env):
    pass


def get_doc_classes():
    return [
        "Hitbox3D",
        "Hurtbox3D",
        "Projectile3D",
    ]


def get_doc_path():
    return "doc_classes"


def get_icons_path():
    return "editor/icons"
