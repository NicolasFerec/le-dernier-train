"""Authored practical lights; shared by map builders and the lighting-only migration."""
import unreal as ue

ACTORS = ue.get_editor_subsystem(ue.EditorActorSubsystem)


def point(component, lumens, radius, color=(255, 206, 145)):
    component.set_mobility(ue.ComponentMobility.MOVABLE)
    component.set_editor_property('use_inverse_squared_falloff', True)
    component.set_editor_property('intensity_units', ue.LightUnits.LUMENS)
    component.set_editor_property('intensity', lumens)
    component.set_editor_property('attenuation_radius', radius)
    component.set_editor_property('light_color', ue.Color(r=color[0], g=color[1], b=color[2], a=255))
    component.set_editor_property('cast_shadows', True)
    component.set_editor_property('source_radius', 6.)
    component.set_editor_property('soft_source_radius', 9.)
    component.set_editor_property('shadow_bias', .25)
    component.set_editor_property('shadow_slope_bias', .35)
    component.set_editor_property('contact_shadow_length', 8.)
    component.set_editor_property('contact_shadow_length_in_ws', True)


def environment(interior=False):
    for actor in ACTORS.get_all_level_actors():
        label = actor.get_actor_label()
        if label == 'Lumière diffuse':
            actor.set_actor_rotation(ue.Rotator(pitch=-52, yaw=-38), False)
            c = actor.light_component
            c.set_editor_property('intensity', .18 if interior else 1.8)
            c.set_editor_property('light_color', ue.Color(r=178, g=204, b=230, a=255))
            c.set_editor_property('cast_shadows', True)
            c.set_editor_property('shadow_bias', .25)
            c.set_editor_property('contact_shadow_length', 8.)
            c.set_editor_property('contact_shadow_length_in_ws', True)
        elif label == 'Lumière de lecture':
            # No omnipresent unshadowed studio light through walls and furniture.
            actor.light_component.set_editor_property('intensity', 0.)
        elif isinstance(actor, ue.SkyLight):
            actor.light_component.set_editor_property('intensity', .65 if interior else .4)
        elif isinstance(actor, ue.PostProcessVolume):
            s = actor.settings
            s.override_bloom_intensity = True
            s.bloom_intensity = .12
            s.override_ambient_occlusion_intensity = True
            s.ambient_occlusion_intensity = .75
            s.override_ambient_occlusion_radius = True
            s.ambient_occlusion_radius = 35.
            s.override_color_saturation = True
            s.color_saturation = ue.Vector4(.96, .96, .96, 1.)
            s.override_vignette_intensity = True
            s.vignette_intensity = .18
            actor.settings = s


def wagon():
    # Replace duplicate floating fills with a source at every visible wall lantern.
    for actor in list(ACTORS.get_all_level_actors()):
        if isinstance(actor, ue.PointLight):
            ACTORS.destroy_actor(actor)
    lanterns = [a for a in ACTORS.get_all_level_actors() if a.get_actor_label() == 'Lantern']
    for fixture in lanterns:
        center, _ = fixture.get_actor_bounds(False)
        # The imported lantern glass is opaque; the small luminous fixture must
        # not occlude its own bulb. Furniture and characters still cast shadows.
        fixture.static_mesh_component.set_editor_property('cast_shadow', False)
        source = ACTORS.spawn_actor_from_class(ue.PointLight, center)
        source.set_actor_label('Lanterne — lumière réelle')
        point(source.point_light_component, 650., 680.)
    environment(True)


def exterior():
    lanterns = [a for a in ACTORS.get_all_level_actors() if a.get_actor_label() == 'Lantern']
    roofs = [a for a in ACTORS.get_all_level_actors() if a.get_actor_label() == 'Marquise d’évacuation']
    for fixture in lanterns:
        center, extent = fixture.get_actor_bounds(False)
        for roof in roofs:
            roof_center, roof_extent = roof.get_actor_bounds(False)
            if abs(center.x-roof_center.x) < roof_extent.x and abs(center.y-roof_center.y) < roof_extent.y:
                ceiling = roof_center.z-roof_extent.z-4.
                if center.z+extent.z > ceiling:
                    position = fixture.get_actor_location()
                    position.z -= center.z+extent.z-ceiling
                    fixture.set_actor_location(position, False, False)
    for actor in ACTORS.get_all_level_actors():
        if isinstance(actor, ue.PointLight):
            c = actor.point_light_component
            # Migrate the old unitless helper once; repeated runs are stable.
            old = not c.use_inverse_squared_falloff
            energy = c.intensity if not old else max(650., min(2400., c.intensity * 180.))
            color = c.light_color
            # FColor's reflected positional arguments are B,G,R,A. The old
            # helper supplied R,G,B,A, turning amber sources blue. Use names.
            rgb = (color.b, color.g, color.r) if old else (color.r, color.g, color.b)
            point(c, energy, max(500., c.attenuation_radius), rgb)
            if lanterns:
                fixture = min(lanterns, key=lambda a: (a.get_actor_location() - actor.get_actor_location()).length())
                if (fixture.get_actor_location() - actor.get_actor_location()).length() < 220.:
                    center, _ = fixture.get_actor_bounds(False)
                    actor.set_actor_location(center, False, False)
                    fixture.static_mesh_component.set_editor_property('cast_shadow', False)
    environment(False)
