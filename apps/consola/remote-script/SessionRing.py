# SessionRing.py - Session Ring Management
"""
Manages the 4x8 session window (ring) for Push-style navigation
"""

from .consts import *
from .MIDIUtils import SysExEncoder, ColorUtils

class SessionRing:
    """
    Manages the 4x8 session ring (window) for clip launching
    Provides navigation and track/scene selection
    """
    
    def __init__(self, control_surface):
        self.c_surface = control_surface
        self.song = control_surface.song()
        self._session = getattr(control_surface, '_session', None)
        
        self.c_surface.log_message("🔧 Initializing SessionRing...")
        
        # Ring dimensions (tracks x scenes)
        self.ring_width = control_surface.ring_width
        self.ring_height = control_surface.ring_height

        # Ring offsets (track/scene indices for the window)
        self.track_offset = 0
        self.scene_offset = 0

        # Selection tracking
        self.selected_track_index = 0
        self.selected_scene_index = 0

        # Session Overview mode (Push 3 style zoom-out)
        self.overview_mode = False
        self.overview_zoom = 4  # Each pad represents 4x4 cells in overview
        
        # Listeners
        self._listeners = []
        self._is_active = False
        
    def set_session_component(self, session_component):
        """Assign SessionComponent once it has been created by PushClone."""
        self._session = session_component
        if session_component:
            self.track_offset = session_component.track_offset()
            self.scene_offset = session_component.scene_offset()

    def setup_listeners(self):
        """Setup ring-related listeners"""
        if self._session is None:
            self.c_surface.log_message("⚠️ SessionRing setup skipped: SessionComponent not ready yet")
            return
        if self._is_active:
            return
            
        try:
            self.c_surface.log_message("🎯 Setting up Session Ring listeners...")
            
            # Track selection changes
            track_listener = lambda: self._on_selected_track_changed()
            self.song.view.add_selected_track_listener(track_listener)
            self._listeners.append(('selected_track', track_listener))
            
            # Scene selection changes
            scene_listener = lambda: self._on_selected_scene_changed()
            self.song.view.add_selected_scene_listener(scene_listener)
            self._listeners.append(('selected_scene', scene_listener))
            
            # Track list changes (tracks added/removed)
            tracks_listener = lambda: self._on_tracks_changed()
            self.song.add_tracks_listener(tracks_listener)
            self._listeners.append(('tracks', tracks_listener))
            
            # Scene list changes (scenes added/removed)
            scenes_listener = lambda: self._on_scenes_changed()
            self.song.add_scenes_listener(scenes_listener)
            self._listeners.append(('scenes', scenes_listener))
            
            # Initialize ring position based on current selection
            self.track_offset = self._session.track_offset()
            self.scene_offset = self._session.scene_offset()
            self._update_ring_from_selection()
            
            self._is_active = True
            self.c_surface.log_message("✅ Session Ring listeners setup")
            
        except Exception as e:
            self.c_surface.log_message(f"❌ Error setting up ring listeners: {e}")
    
    def cleanup_listeners(self):
        """Remove all ring listeners"""
        if not self._is_active:
            return
            
        try:
            for listener_type, listener_func in self._listeners:
                try:
                    if listener_type == 'selected_track':
                        self.song.view.remove_selected_track_listener(listener_func)
                    elif listener_type == 'selected_scene':
                        self.song.view.remove_selected_scene_listener(listener_func)
                    elif listener_type == 'tracks':
                        self.song.remove_tracks_listener(listener_func)
                    elif listener_type == 'scenes':
                        self.song.remove_scenes_listener(listener_func)
                except:
                    pass
            
            self._listeners = []
            self._is_active = False
            self.c_surface.log_message("✅ Session Ring listeners cleaned up")
            
        except Exception as e:
            self.c_surface.log_message(f"❌ Error cleaning ring listeners: {e}")
    
    # ========================================
    # RING POSITION MANAGEMENT
    # ========================================
    
    def _update_ring_from_selection(self):
        """Update ring position to follow selected track/scene"""
        try:
            # Cache current offsets from SessionComponent
            self.track_offset = self._session.track_offset()
            self.scene_offset = self._session.scene_offset()

            # Inform hardware about the new ring window
            self._send_ring_position()
            self._send_ring_tracks()
            self._send_ring_scenes()
            self._send_ring_clips()

            self._ensure_clip_region_monitored()

        except Exception as e:
            self.c_surface.log_message(f"❌ Error updating ring from selection: {e}")
    
    def navigate_ring(self, direction):
        """
        Navigate ring in specified direction
        Args:
            direction (str): 'left', 'right', 'up', 'down'
        """
        try:
            track_offset = self._session.track_offset()
            scene_offset = self._session.scene_offset()

            # Calculate max offsets to keep ring fully visible
            num_tracks = len(self.song.tracks)
            num_scenes = len(self.song.scenes)
            max_track_offset = max(0, num_tracks - self.ring_width)
            max_scene_offset = max(0, num_scenes - self.ring_height)

            self.c_surface.log_message(
                f"🔵 Navigate {direction}: Current T{track_offset} S{scene_offset} "
                f"(limits: T0-{max_track_offset}, S0-{max_scene_offset})"
            )

            # Calculate new offset with clamping
            new_track_offset = track_offset
            new_scene_offset = scene_offset

            if direction == 'left':
                new_track_offset = max(0, track_offset - 1)
            elif direction == 'right':
                new_track_offset = min(max_track_offset, track_offset + 1)
            elif direction == 'up':
                new_scene_offset = max(0, scene_offset - 1)
            elif direction == 'down':
                new_scene_offset = min(max_scene_offset, scene_offset + 1)

            # Apply new offsets
            self._session.set_offsets(new_track_offset, new_scene_offset)

            # Verify actual offsets after SessionComponent applies them
            actual_track_offset = self._session.track_offset()
            actual_scene_offset = self._session.scene_offset()

            self.c_surface.log_message(
                f"🔵 Requested T{new_track_offset} S{new_scene_offset} → "
                f"Got T{actual_track_offset} S{actual_scene_offset}"
            )

            if (track_offset != actual_track_offset or scene_offset != actual_scene_offset):

                # Update self properties before sending updates
                self.track_offset = actual_track_offset
                self.scene_offset = actual_scene_offset

                self.c_surface.log_message(
                    f"🎯 Ring navigated {direction}: T{actual_track_offset} S{actual_scene_offset} "
                    f"(ring: {self.ring_width}x{self.ring_height})"
                )

                # Force highlight update in Ableton
                self.c_surface.set_session_highlight(
                    actual_track_offset,
                    actual_scene_offset,
                    self.ring_width,
                    self.ring_height,
                    include_returns=False
                )

                self._send_ring_position()

                # Send track/scene metadata and clips directly (no coalescer)
                # This ensures immediate delivery during navigation
                self._send_ring_tracks()
                self._send_ring_scenes()
                self._send_ring_clips()

                self._ensure_clip_region_monitored()
            else:
                self.c_surface.log_message(f"⚠️ Ring did NOT move (already at boundary)")

        except Exception as e:
            self.c_surface.log_message(f"❌ Error navigating ring {direction}: {e}")
    
    def _update_selection_to_ring(self):
        """Update Live's selection to match ring position"""
        try:
            # Select track at current ring position if different
            current_track_idx = self.track_offset + (self.ring_width // 2)  # Center of ring
            current_scene_idx = self.scene_offset + (self.ring_height // 2)  # Center of ring
            
            if (current_track_idx < len(self.song.tracks) and
                current_track_idx != self.selected_track_index):
                self.song.view.selected_track = self.song.tracks[current_track_idx]
                
            if (current_scene_idx < len(self.song.scenes) and
                current_scene_idx != self.selected_scene_index):
                self.song.view.selected_scene = self.song.scenes[current_scene_idx]
                
        except Exception as e:
            self.c_surface.log_message(f"❌ Error updating selection to ring: {e}")
    
    # ========================================
    # EVENT HANDLERS
    # ========================================
    
    def _on_selected_track_changed(self):
        """Handle track selection changes"""
        self.c_surface.log_message(f"🔔 _on_selected_track_changed called (connected={self.c_surface._is_connected})")

        if self.c_surface._is_connected:
            try:
                selected_track = self.song.view.selected_track
                track_index = 0

                for i, track in enumerate(self.song.tracks):
                    if track == selected_track:
                        track_index = i
                        break

                old_index = self.selected_track_index
                self.selected_track_index = track_index

                self.c_surface.log_message(f"🔵 Track changed: {old_index} → {track_index}")

                if track_index != old_index:
                    self.c_surface.log_message(f"🎯 Track selection changed: {track_index}")
                    self._update_ring_from_selection()
                    self._send_track_selection(track_index)
                else:
                    self.c_surface.log_message(f"⚠️ Track index same, not sending update")

            except Exception as e:
                self.c_surface.log_message(f"❌ Error handling track selection: {e}")
    
    def _on_selected_scene_changed(self):
        """Handle scene selection changes"""
        if self.c_surface._is_connected:
            try:
                selected_scene = self.song.view.selected_scene
                scene_index = 0
                
                for i, scene in enumerate(self.song.scenes):
                    if scene == selected_scene:
                        scene_index = i
                        break
                
                old_index = self.selected_scene_index
                self.selected_scene_index = scene_index
                
                if scene_index != old_index:
                    self.c_surface.log_message(f"🎯 Scene selection changed: {scene_index}")
                    self._update_ring_from_selection()
                    self._send_scene_selection(scene_index)
                
            except Exception as e:
                self.c_surface.log_message(f"❌ Error handling scene selection: {e}")
    
    def _on_tracks_changed(self):
        """Handle tracks added/removed"""
        if self.c_surface._is_connected:
            self.c_surface.log_message("🎯 Tracks changed, updating ring")
            # Re-clamp ring position
            max_track_offset = max(0, len(self.song.tracks) - self.ring_width)
            self.track_offset = min(self.track_offset, max_track_offset)
            self._send_ring_position()
            # Use legacy methods (bulk commands too large for USB MIDI buffer)
            self._send_ring_tracks()
            self._send_ring_scenes()
            self._send_ring_clips()

    def _on_scenes_changed(self):
        """Handle scenes added/removed"""
        if self.c_surface._is_connected:
            self.c_surface.log_message("🎯 Scenes changed, updating ring")
            # Re-clamp ring position
            max_scene_offset = max(0, len(self.song.scenes) - self.ring_height)
            self.scene_offset = min(self.scene_offset, max_scene_offset)
            self._send_ring_position()
            # Send directly (no coalescer needed)
            self._send_ring_tracks()
            self._send_ring_scenes()
            self._send_ring_clips()
    
    # ========================================
    # SEND METHODS
    # ========================================
    
    def _send_ring_position(self):
        """Send current ring position to hardware using 14-bit encoding"""
        try:
            from .MIDIUtils import SysExEncoder

            # Use 14-bit encoding for offsets (supports up to 16,384 tracks/scenes)
            # MSB = upper 7 bits, LSB = lower 7 bits
            track_offset_msb = (self.track_offset >> 7) & 0x7F
            track_offset_lsb = self.track_offset & 0x7F
            scene_offset_msb = (self.scene_offset >> 7) & 0x7F
            scene_offset_lsb = self.scene_offset & 0x7F

            # Payload: [track_msb, track_lsb, scene_msb, scene_lsb, width, height, overview_mode]
            payload = [
                track_offset_msb,
                track_offset_lsb,
                scene_offset_msb,
                scene_offset_lsb,
                self.ring_width & 0x7F,
                self.ring_height & 0x7F,
                1 if self.overview_mode else 0
            ]
            self.c_surface._send_sysex_command(CMD_RING_POSITION, payload)
            
        except Exception as e:
            self.c_surface.log_message(f"❌ Error sending ring position: {e}")
    
    def _send_ring_tracks(self, use_delays=True):
        """Send track names and colors for visible tracks in current ring.

        Args:
            use_delays: If True, add small delays between messages to prevent USB buffer overflow.
                       Set to False for handshake where timing is less critical.
        """
        try:
            import time

            if not self.c_surface._is_connected:
                return

            # Send track info for each visible track in the ring
            # Send directly without coalescer to ensure immediate delivery
            num_tracks = len(self.song.tracks)
            for ring_track in range(self.ring_width):
                absolute_track = self.track_offset + ring_track
                if absolute_track < num_tracks:
                    track = self.song.tracks[absolute_track]

                    # Send track name directly
                    name_bytes = track.name.encode('utf-8')[:12]
                    payload = [absolute_track, len(name_bytes)]
                    payload.extend(list(name_bytes))
                    self.c_surface._send_sysex_command(CMD_TRACK_NAME, payload)
                    if use_delays:
                        time.sleep(0.003)  # 3ms delay during navigation

                    # Send track color directly
                    color_rgb = ColorUtils.live_color_to_rgb(track.color)
                    r = min(127, max(0, color_rgb[0] // 2))
                    g = min(127, max(0, color_rgb[1] // 2))
                    b = min(127, max(0, color_rgb[2] // 2))
                    payload = [absolute_track, r, g, b]
                    self.c_surface._send_sysex_command(CMD_TRACK_COLOR, payload)
                    if use_delays:
                        time.sleep(0.003)  # 3ms delay during navigation

                    self.c_surface.log_message(f"📤 Sent T{absolute_track} color RGB({color_rgb[0]},{color_rgb[1]},{color_rgb[2]}) directly")

            self.c_surface.log_message(
                f"✅ Sent track info for ring tracks {self.track_offset}-{self.track_offset + self.ring_width - 1}"
            )

        except Exception as e:
            self.c_surface.log_message(f"❌ Error sending ring tracks: {e}")

    def _send_ring_scenes(self, use_delays=True):
        """Send scene names and colors for visible scenes in current ring.

        Args:
            use_delays: If True, add small delays between messages to prevent USB buffer overflow.
                       Set to False for handshake where timing is less critical.
        """
        try:
            import time

            if not self.c_surface._is_connected:
                return

            # Send scene info for each visible scene in the ring
            # Send directly without coalescer to ensure immediate delivery
            num_scenes = len(self.song.scenes)
            for ring_scene in range(self.ring_height):
                absolute_scene = self.scene_offset + ring_scene
                if absolute_scene < num_scenes:
                    scene = self.song.scenes[absolute_scene]

                    # Send scene name directly
                    name_bytes = scene.name.encode('utf-8')[:12]
                    payload = [absolute_scene, len(name_bytes)]
                    payload.extend(list(name_bytes))
                    self.c_surface._send_sysex_command(CMD_SCENE_NAME, payload)
                    if use_delays:
                        time.sleep(0.003)  # 3ms delay during navigation

                    # Send scene color directly
                    color_rgb = ColorUtils.live_color_to_rgb(scene.color)
                    r = min(127, max(0, color_rgb[0] // 2))
                    g = min(127, max(0, color_rgb[1] // 2))
                    b = min(127, max(0, color_rgb[2] // 2))
                    payload = [absolute_scene, r, g, b]
                    self.c_surface._send_sysex_command(CMD_SCENE_COLOR, payload)
                    if use_delays:
                        time.sleep(0.003)  # 3ms delay during navigation

            self.c_surface.log_message(
                f"✅ Sent scene info for ring scenes {self.scene_offset}-{self.scene_offset + self.ring_height - 1}"
            )

        except Exception as e:
            self.c_surface.log_message(f"❌ Error sending ring scenes: {e}")

    def _send_ring_clips(self):
        """Send all clips in current ring to hardware using a single grid message."""
        try:
            self.c_surface.log_message(
                f"🔵 _send_ring_clips called: T{self.track_offset} S{self.scene_offset} "
                f"({self.ring_width}x{self.ring_height}) connected={self.c_surface._is_connected}"
            )

            if not self.c_surface._is_connected:
                self.c_surface.log_message("⚠️ Not connected, skipping ring clips")
                return

            # Get the clip manager and send the single, consolidated grid update
            clip_manager = self.c_surface.get_manager('clip')
            self.c_surface.log_message(f"🔵 Got clip_manager: {clip_manager is not None}")

            if clip_manager:
                self.c_surface.log_message(
                    f"🔵 Calling clip_manager._send_neotrellis_clip_grid(track_start={self.track_offset}, scene_start={self.scene_offset})"
                )
                clip_manager._send_neotrellis_clip_grid(
                    track_start=self.track_offset,
                    scene_start=self.scene_offset
                )
                self.c_surface.log_message("✅ Grid update sent successfully")
            else:
                self.c_surface.log_message("⚠️ clip_manager is None!")

        except Exception as e:
            self.c_surface.log_message(f"❌ Error sending ring clips: {e}")

    # ========================================
    # BULK SESSION RING COMMANDS
    # ========================================

    def _send_complete_ring_bulk(self):
        """Send complete session ring state using bulk commands (optimized)"""
        try:
            if not self.c_surface._is_connected:
                return

            self.c_surface.log_message(
                f"📦 Sending bulk session ring: T{self.track_offset} S{self.scene_offset}"
            )

            # Send metadata (tracks + scenes with names/colors)
            self._send_ring_metadata_bulk()

            # Send clips (32 clips with states/colors)
            self._send_ring_clips_bulk()

            self.c_surface.log_message("✅ Bulk session ring sent")

        except Exception as e:
            self.c_surface.log_message(f"❌ Error sending bulk ring: {e}")

    def _send_ring_metadata_bulk(self):
        """
        Send bulk metadata for visible tracks and scenes
        Format: [num_tracks] [track0: len, name..., R, G, B] ... [track7: ...]
                [num_scenes] [scene0: len, name..., R, G, B] ... [scene3: ...]
        """
        try:
            if not self.c_surface._is_connected:
                return

            payload = []
            num_tracks = len(self.song.tracks)
            num_scenes = len(self.song.scenes)

            # Add visible tracks count
            visible_tracks = min(self.ring_width, num_tracks - self.track_offset)
            payload.append(visible_tracks & 0x7F)

            # Add track data (name + color)
            for ring_track in range(visible_tracks):
                absolute_track = self.track_offset + ring_track
                if absolute_track < num_tracks:
                    track = self.song.tracks[absolute_track]

                    # Track name (max 12 chars)
                    name = str(track.name)[:12]
                    name_bytes = [ord(c) & 0x7F for c in name]
                    payload.append(len(name_bytes) & 0x7F)
                    payload.extend(name_bytes)

                    # Track color (RGB 7-bit)
                    color_rgb = ColorUtils.live_color_to_rgb(track.color)
                    payload.append(color_rgb[0] >> 1)  # R (7-bit)
                    payload.append(color_rgb[1] >> 1)  # G (7-bit)
                    payload.append(color_rgb[2] >> 1)  # B (7-bit)

            # Add visible scenes count
            visible_scenes = min(self.ring_height, num_scenes - self.scene_offset)
            payload.append(visible_scenes & 0x7F)

            # Add scene data (name + color)
            for ring_scene in range(visible_scenes):
                absolute_scene = self.scene_offset + ring_scene
                if absolute_scene < num_scenes:
                    scene = self.song.scenes[absolute_scene]

                    # Scene name (max 12 chars)
                    name = str(scene.name)[:12]
                    name_bytes = [ord(c) & 0x7F for c in name]
                    payload.append(len(name_bytes) & 0x7F)
                    payload.extend(name_bytes)

                    # Scene color (RGB 7-bit)
                    color_rgb = ColorUtils.live_color_to_rgb(scene.color)
                    payload.append(color_rgb[0] >> 1)  # R (7-bit)
                    payload.append(color_rgb[1] >> 1)  # G (7-bit)
                    payload.append(color_rgb[2] >> 1)  # B (7-bit)

            # Send bulk metadata command (high priority to bypass coalescer)
            self.c_surface._send_sysex_command(CMD_SESSION_RING_METADATA, payload, priority=1)
            self.c_surface.log_message(
                f"📦 Sent metadata bulk: {visible_tracks} tracks, {visible_scenes} scenes ({len(payload)} bytes)"
            )

        except Exception as e:
            self.c_surface.log_message(f"❌ Error sending ring metadata bulk: {e}")

    def _send_ring_clips_bulk(self):
        """
        Send bulk clip data for 8x4 grid
        Format: [clip0: state, R, G, B] [clip1: ...] ... [clip31: ...]
        Order: column-major (track 0 scenes 0-3, track 1 scenes 0-3, ...)
        """
        try:
            if not self.c_surface._is_connected:
                return

            payload = []
            num_tracks = len(self.song.tracks)
            num_scenes = len(self.song.scenes)

            # Send all 32 clips in column-major order
            for ring_track in range(self.ring_width):
                for ring_scene in range(self.ring_height):
                    absolute_track = self.track_offset + ring_track
                    absolute_scene = self.scene_offset + ring_scene

                    # Default: empty clip
                    clip_state = CLIP_EMPTY
                    color = (0, 0, 0)

                    # Check if track and scene exist
                    if absolute_track < num_tracks and absolute_scene < num_scenes:
                        track = self.song.tracks[absolute_track]
                        clip_slot = track.clip_slots[absolute_scene]

                        if clip_slot.has_clip:
                            clip = clip_slot.clip

                            # Determine clip state
                            if clip_slot.is_recording:
                                clip_state = CLIP_RECORDING
                            elif clip_slot.is_playing:
                                clip_state = CLIP_PLAYING
                            elif clip_slot.is_triggered:
                                clip_state = CLIP_QUEUED
                            else:
                                clip_state = CLIP_STOPPED

                            # Get clip color
                            color = ColorUtils.live_color_to_rgb(clip.color)

                    # Add clip data: [state, R7, G7, B7]
                    payload.append(clip_state & 0x7F)
                    payload.append(color[0] >> 1)  # R (7-bit)
                    payload.append(color[1] >> 1)  # G (7-bit)
                    payload.append(color[2] >> 1)  # B (7-bit)

            # Send bulk clips command (high priority to bypass coalescer)
            self.c_surface._send_sysex_command(CMD_SESSION_RING_CLIPS, payload, priority=1)
            self.c_surface.log_message(
                f"📦 Sent clips bulk: 32 clips ({len(payload)} bytes)"
            )

        except Exception as e:
            self.c_surface.log_message(f"❌ Error sending ring clips bulk: {e}")

    def _send_track_selection(self, track_index):
        """Send track selection change to hardware"""
        try:
            # Use simple 7-bit encoding (tracks 0-127)
            # CMD_SELECTED_TRACK expects single byte: [track_index]
            payload = [track_index & 0x7F]

            self.c_surface.log_message(f"📤 Sending CMD_SELECTED_TRACK: track {track_index}")
            self.c_surface._send_sysex_command(CMD_SELECTED_TRACK, payload)

        except Exception as e:
            self.c_surface.log_message(f"❌ Error sending track selection: {e}")
    
    def _send_scene_selection(self, scene_index):
        """Send scene selection change to hardware with 14-bit encoding"""
        try:
            # Calculate relative position within ring
            relative_scene = scene_index - self.scene_offset
            is_in_ring = 0 <= relative_scene < self.ring_height

            # Use 14-bit encoding for scene index
            scene_msb = (scene_index >> 7) & 0x7F
            scene_lsb = scene_index & 0x7F

            # Payload: [scene_msb, scene_lsb, is_in_ring]
            payload = [scene_msb, scene_lsb, 1 if is_in_ring else 0]
            self.c_surface._send_sysex_command(CMD_SCENE_SELECT, payload)
                
        except Exception as e:
            self.c_surface.log_message(f"❌ Error sending scene selection: {e}")
    
    # ========================================
    # SESSION OVERVIEW MODE (Push 3 style)
    # ========================================

    def toggle_overview_mode(self):
        """Toggle Session Overview mode on/off"""
        try:
            self.overview_mode = not self.overview_mode
            state = "enabled" if self.overview_mode else "disabled"
            self.c_surface.log_message(f"🔍 Session Overview: {state}")

            if self.overview_mode:
                self._send_overview_grid()
            else:
                # Return to normal mode
                self._send_ring_position()
                self._send_ring_clips()

        except Exception as e:
            self.c_surface.log_message(f"❌ Error toggling overview mode: {e}")

    def _send_overview_grid(self):
        """
        Send overview grid to hardware
        Each pad represents multiple tracks/scenes (zoom-out view)

        Example with zoom=4:
        - Pad (0,0) represents tracks 0-3, scenes 0-3
        - Pad (1,0) represents tracks 4-7, scenes 0-3
        - etc.
        """
        try:
            if not self.c_surface._is_connected:
                return

            # Build overview grid (4x8 pads)
            grid_data = []

            for scene_idx in range(self.ring_height):  # 8 scenes
                for track_idx in range(self.ring_width):  # 4 tracks
                    # Calculate absolute track/scene range this pad represents
                    abs_track_start = track_idx * self.overview_zoom
                    abs_track_end = abs_track_start + self.overview_zoom
                    abs_scene_start = scene_idx * self.overview_zoom
                    abs_scene_end = abs_scene_start + self.overview_zoom

                    # Check if this region has any clips
                    has_clips = self._count_clips_in_region(
                        abs_track_start, abs_track_end,
                        abs_scene_start, abs_scene_end
                    )

                    # Determine color based on clip density
                    if has_clips == 0:
                        # No clips - dark gray
                        color = (20, 20, 20)
                    elif has_clips < self.overview_zoom * self.overview_zoom // 2:
                        # Few clips - medium gray
                        color = (60, 60, 60)
                    elif has_clips < self.overview_zoom * self.overview_zoom:
                        # Many clips - bright gray
                        color = (100, 100, 100)
                    else:
                        # Full - white
                        color = (127, 127, 127)

                    # Check if this region contains currently selected track/scene
                    if (abs_track_start <= self.selected_track_index < abs_track_end and
                        abs_scene_start <= self.selected_scene_index < abs_scene_end):
                        # Highlight selection in blue
                        color = (50, 100, 255)

                    grid_data.extend(color)

            # Send overview grid using CMD_SESSION_OVERVIEW_GRID
            self.c_surface._send_sysex_command(CMD_SESSION_OVERVIEW_GRID, grid_data)

        except Exception as e:
            self.c_surface.log_message(f"❌ Error sending overview grid: {e}")

    def _count_clips_in_region(self, track_start, track_end, scene_start, scene_end):
        """Count number of clips in a region"""
        try:
            clip_count = 0

            for track_idx in range(track_start, track_end):
                if track_idx >= len(self.song.tracks):
                    break

                track = self.song.tracks[track_idx]

                for scene_idx in range(scene_start, scene_end):
                    if scene_idx >= len(track.clip_slots):
                        break

                    clip_slot = track.clip_slots[scene_idx]
                    if clip_slot.has_clip:
                        clip_count += 1

            return clip_count

        except Exception as e:
            return 0

    def handle_overview_pad_press(self, ring_track, ring_scene):
        """
        Handle pad press in overview mode
        Zoom into the region represented by this pad
        """
        try:
            # Calculate absolute position this pad represents
            abs_track = ring_track * self.overview_zoom
            abs_scene = ring_scene * self.overview_zoom

            # Exit overview mode
            self.overview_mode = False

            # Move ring to this position
            self.track_offset = abs_track
            self.scene_offset = abs_scene

            # Clamp to valid ranges
            max_track_offset = max(0, len(self.song.tracks) - self.ring_width)
            max_scene_offset = max(0, len(self.song.scenes) - self.ring_height)
            self.track_offset = min(self.track_offset, max_track_offset)
            self.scene_offset = min(self.scene_offset, max_scene_offset)

            # Send updated state
            self._send_ring_position()
            self._send_ring_clips()

            self.c_surface.log_message(
                f"🔍 Zoomed to T{self.track_offset} S{self.scene_offset}"
            )

        except Exception as e:
            self.c_surface.log_message(f"❌ Error handling overview pad: {e}")

    # ========================================
    # PUBLIC INTERFACE
    # ========================================
    
    def get_ring_info(self):
        """Get current ring information"""
        return {
            'track_offset': self.track_offset,
            'scene_offset': self.scene_offset,
            'ring_width': self.ring_width,
            'ring_height': self.ring_height,
            'selected_track': self.selected_track_index,
            'selected_scene': self.selected_scene_index,
            'total_tracks': len(self.song.tracks),
            'total_scenes': len(self.song.scenes),
            'overview_mode': self.overview_mode,
            'overview_zoom': self.overview_zoom,
            'max_visible_tracks': self.ring_width * self.overview_zoom if self.overview_mode else self.ring_width,
            'max_visible_scenes': self.ring_height * self.overview_zoom if self.overview_mode else self.ring_height
        }
    
    def get_absolute_position(self, ring_track, ring_scene):
        """Convert ring-relative position to absolute track/scene indices"""
        return (self.track_offset + ring_track, self.scene_offset + ring_scene)
    
    def get_ring_position(self, absolute_track, absolute_scene):
        """Convert absolute position to ring-relative position"""
        ring_track = absolute_track - self.track_offset
        ring_scene = absolute_scene - self.scene_offset
        
        # Return None if outside ring
        if (ring_track < 0 or ring_track >= self.ring_width or
            ring_scene < 0 or ring_scene >= self.ring_height):
            return None
            
        return (ring_track, ring_scene)
    
    def is_in_ring(self, absolute_track, absolute_scene):
        """Check if absolute position is within current ring"""
        return (self.track_offset <= absolute_track < self.track_offset + self.ring_width and
                self.scene_offset <= absolute_scene < self.scene_offset + self.ring_height)
    
    def send_complete_state(self):
        """Send complete ring state to hardware"""
        if not self.c_surface._is_connected:
            return

        try:
            self.c_surface.log_message("=" * 60)
            self.c_surface.log_message("🎯 Sending complete ring state...")
            self.c_surface.log_message("✅ DIRECT SEND (v2.3 - HANDSHAKE NO DELAYS)")
            self.c_surface.log_message("=" * 60)
            self._send_ring_position()
            # Send directly WITHOUT delays during handshake (timing less critical)
            self._send_ring_tracks(use_delays=False)
            self._send_ring_scenes(use_delays=False)
            self._send_ring_clips()

            self._send_track_selection(self.selected_track_index)
            self._send_scene_selection(self.selected_scene_index)
            self._ensure_clip_region_monitored()

        except Exception as e:
            self.c_surface.log_message(f"❌ Error sending ring state: {e}")

    def _ensure_clip_region_monitored(self):
        """Instruct ClipManager to monitor the current ring window."""
        try:
            clip_manager = self.c_surface.get_manager('clip')
            if clip_manager and hasattr(clip_manager, 'ensure_region_monitored'):
                clip_manager.ensure_region_monitored(
                    self.track_offset,
                    self.ring_width,
                    self.scene_offset,
                    self.ring_height
                )
        except Exception as e:
            self.c_surface.log_message(f"❌ Error ensuring clip region monitored: {e}")
    
    def handle_navigation_command(self, command, payload):
        """Handle navigation commands from hardware"""
        try:
            self.c_surface.log_message(f"🔵 SessionRing received CMD:0x{command:02X} with {len(payload)} bytes")

            if command == CMD_RING_NAVIGATE and len(payload) >= 1:
                direction_map = {0: 'left', 1: 'right', 2: 'up', 3: 'down'}
                direction = direction_map.get(payload[0])
                self.c_surface.log_message(f"🔵 CMD_RING_NAVIGATE: payload[0]={payload[0]} → direction='{direction}'")
                if direction:
                    self.navigate_ring(direction)
                else:
                    self.c_surface.log_message(f"⚠️ Unknown direction value: {payload[0]}")

            elif command == CMD_RING_SELECT and len(payload) >= 2:
                ring_track, ring_scene = payload[0], payload[1]

                # Check if in overview mode
                if self.overview_mode:
                    # Handle overview pad press (zoom in)
                    self.handle_overview_pad_press(ring_track, ring_scene)
                else:
                    # Normal mode - update selection
                    absolute_track, absolute_scene = self.get_absolute_position(ring_track, ring_scene)

                    # Update Live's selection
                    if absolute_track < len(self.song.tracks):
                        self.song.view.selected_track = self.song.tracks[absolute_track]
                    if absolute_scene < len(self.song.scenes):
                        self.song.view.selected_scene = self.song.scenes[absolute_scene]

            elif command == CMD_SESSION_OVERVIEW:
                # Toggle overview mode
                self.toggle_overview_mode()

            elif command == CMD_TRACK_SELECT and len(payload) >= 1:
                # Select track by absolute index (from GUI)
                track_index = payload[0] & 0x7F  # 7-bit encoding (0-127)
                self.c_surface.log_message(f"🔵 CMD_TRACK_SELECT: requesting track {track_index}")

                if track_index < len(self.song.tracks):
                    self.song.view.selected_track = self.song.tracks[track_index]
                    self.c_surface.log_message(f"✅ Track {track_index} selected")
                else:
                    self.c_surface.log_message(f"⚠️ Track {track_index} out of range (max: {len(self.song.tracks) - 1})")

        except Exception as e:
            self.c_surface.log_message(f"❌ Error handling navigation command 0x{command:02X}: {e}")
