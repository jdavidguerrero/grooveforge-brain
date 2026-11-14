# MessageCoalescer.py - Performance-optimized message batching
"""
Manages message coalescing for optimal hardware communication performance
Limits refresh rate to 60fps and batches LED updates
"""

import time
from threading import Timer
from .consts import *
from .MIDIUtils import SysExEncoder

class MessageCoalescer:
    """
    Coalesces messages for optimal performance
    - Groups LED updates in time windows (2-4ms)
    - Limits refresh rate to 60fps max
    - Prevents message flooding
    """
    
    def __init__(self, control_surface):
        self.c_surface = control_surface
        
        self.c_surface.log_message("🔧 Initializing MessageCoalescer...")
        
        # Performance settings
        self.target_fps = 60
        self.frame_time_ms = 1000.0 / self.target_fps  # ~16.67ms per frame
        self.coalesce_window_ms = 4.0  # Group messages within 4ms
        
        # Message batching
        self._pending_messages = {}  # state_key -> latest message data
        self._last_send_time = 0
        self._flush_timer = None
        self._frame_timer = None
        
        # Message priorities (lower number = higher priority)
        self._priority_commands = {
            CMD_TRANSPORT_STATE: 1,      # Highest - transport changes
            CMD_RING_POSITION: 1,        # Highest - ring navigation
            CMD_CLIP_STATE: 2,           # High - clip state/color
            CMD_TRACK_NAME: 2,           # High - track metadata
            CMD_TRACK_COLOR: 2,
            CMD_MIXER_MUTE: 2,           # High - mixer state
            CMD_MIXER_SOLO: 2,
            CMD_MIXER_ARM: 2,
            CMD_DEVICE_PARAMS: 2,        # High - device params
            CMD_CLIP_NAME: 3,            # Medium - names/colors
            CMD_SCENE_NAME: 3,
            CMD_SCENE_COLOR: 3,
            CMD_MIXER_VOLUME: 3,         # Medium - continuous controllers
            CMD_MIXER_PAN: 3,
            CMD_MIXER_SEND: 3,
            CMD_STEP_SEQUENCER_STATE: 3,
            CMD_NEOTRELLIS_GRID: 3       # Medium - bulk LED updates
        }
        
        # Statistics
        self._messages_coalesced = 0
        self._messages_sent = 0
        self._frames_dropped = 0
        
        # State tracking for deltas
        self._last_states = {}  # state_key -> last_payload
        
    def queue_message(self, command, payload, priority_override=None, state_key=None):
        """Queue a message for coalesced sending"""
        try:
            current_time = time.time() * 1000  # Convert to ms
            
            # Check if this is a duplicate of the last state
            key = state_key or command
            if self._is_duplicate_state(key, payload):
                return  # Skip duplicate
            
            # Determine message priority
            priority = priority_override or self._priority_commands.get(command, 4)
            
            # Store message with metadata
            message_data = {
                'payload': payload,
                'priority': priority,
                'timestamp': current_time,
                'command': command,
                'state_key': key
            }
            
            # For LED updates, only keep the latest
            if self._is_led_command(command):
                self._pending_messages[key] = message_data
            else:
                # For other commands, create unique key to avoid overwriting
                unique_key = f"{key}_{len(self._pending_messages)}"
                self._pending_messages[unique_key] = message_data
            
            self._messages_coalesced += 1
            
            # Schedule flush if not already scheduled
            self._schedule_flush()
            
        except Exception as e:
            self.c_surface.log_message(f"❌ Error queuing message: {e}")
    
    def _is_duplicate_state(self, state_key, payload):
        """Check if this payload is identical to the last one sent"""
        try:
            last_payload = self._last_states.get(state_key)
            if last_payload is None:
                return False
                
            # Compare payloads
            if len(payload) != len(last_payload):
                return False
                
            for i, byte in enumerate(payload):
                if byte != last_payload[i]:
                    return False
                    
            return True  # Identical payload
            
        except Exception:
            return False
    
    def _is_led_command(self, command):
        """Check if command is an LED/visual update that can be coalesced"""
        led_commands = {
            CMD_CLIP_STATE,
            CMD_CLIP_NAME,
            CMD_NEOTRELLIS_GRID,
            CMD_NEOTRELLIS_CLIP_GRID,
            CMD_STEP_SEQUENCER_STATE,
            CMD_DEVICE_PARAMS,
            CMD_MIXER_STATE,
            CMD_MIXER_VOLUME,
            CMD_MIXER_PAN,
            CMD_MIXER_SEND,
            CMD_MIXER_MUTE,
            CMD_MIXER_SOLO,
            CMD_MIXER_ARM,
            CMD_TRANSPORT_STATE,
            CMD_TRACK_NAME,
            CMD_TRACK_COLOR,
            CMD_SCENE_NAME,
            CMD_SCENE_COLOR
        }
        return command in led_commands

    # === TRACK HELPERS ===
    def queue_track_name(self, track_idx, name_bytes):
        """Queue a track name update; state-keyed per track to coalesce."""
        payload = [track_idx, len(name_bytes)]
        payload.extend(name_bytes)
        key = f"{CMD_TRACK_NAME}_T{track_idx}"
        self.queue_message(CMD_TRACK_NAME, payload, priority_override=2, state_key=key)

    def queue_track_color(self, track_idx, r, g, b):
        payload = [track_idx, r, g, b]
        key = f"{CMD_TRACK_COLOR}_T{track_idx}"
        self.queue_message(CMD_TRACK_COLOR, payload, priority_override=2, state_key=key)

    # === CLIP HELPERS ===
    def queue_clip_name(self, track_idx, scene_idx, name_bytes):
        """Queue a clip name update"""
        payload = [track_idx, scene_idx, len(name_bytes)]
        payload.extend(name_bytes)
        key = f"{CMD_CLIP_NAME}_T{track_idx}S{scene_idx}"
        self.queue_message(CMD_CLIP_NAME, payload, priority_override=3, state_key=key)

    def queue_clip_state(self, track_idx, scene_idx, state, r, g, b):
        """Queue a clip state update"""
        # Encode RGB as 14-bit (6 bytes) for higher color fidelity
        r14 = r << 1
        g14 = g << 1
        b14 = b << 1
        payload = [
            track_idx, scene_idx, state,
            (r14 >> 7) & 0x7F, r14 & 0x7F,
            (g14 >> 7) & 0x7F, g14 & 0x7F,
            (b14 >> 7) & 0x7F, b14 & 0x7F
        ]
        key = f"{CMD_CLIP_STATE}_T{track_idx}S{scene_idx}"
        self.queue_message(CMD_CLIP_STATE, payload, priority_override=2, state_key=key)

    # === MIXER HELPERS ===
    def queue_mixer_volume(self, track_idx, value14bit):
        """Queue mixer volume update (14-bit)"""
        msb = (value14bit >> 7) & 0x7F
        lsb = value14bit & 0x7F
        payload = [track_idx, msb, lsb]
        key = f"{CMD_MIXER_VOLUME}_T{track_idx}"
        self.queue_message(CMD_MIXER_VOLUME, payload, priority_override=3, state_key=key)

    def queue_mixer_pan(self, track_idx, value14bit):
        """Queue mixer pan update (14-bit)"""
        msb = (value14bit >> 7) & 0x7F
        lsb = value14bit & 0x7F
        payload = [track_idx, msb, lsb]
        key = f"{CMD_MIXER_PAN}_T{track_idx}"
        self.queue_message(CMD_MIXER_PAN, payload, priority_override=3, state_key=key)

    def queue_mixer_send(self, track_idx, send_idx, value14bit):
        """Queue mixer send update (14-bit)"""
        msb = (value14bit >> 7) & 0x7F
        lsb = value14bit & 0x7F
        payload = [track_idx, send_idx, msb, lsb]
        key = f"{CMD_MIXER_SEND}_T{track_idx}S{send_idx}"
        self.queue_message(CMD_MIXER_SEND, payload, priority_override=3, state_key=key)

    def queue_mixer_mute(self, track_idx, is_muted):
        """Queue mixer mute state"""
        payload = [track_idx, 1 if is_muted else 0]
        key = f"{CMD_MIXER_MUTE}_T{track_idx}"
        self.queue_message(CMD_MIXER_MUTE, payload, priority_override=2, state_key=key)

    def queue_mixer_solo(self, track_idx, is_solo):
        """Queue mixer solo state"""
        payload = [track_idx, 1 if is_solo else 0]
        key = f"{CMD_MIXER_SOLO}_T{track_idx}"
        self.queue_message(CMD_MIXER_SOLO, payload, priority_override=2, state_key=key)

    def queue_mixer_arm(self, track_idx, is_armed):
        """Queue mixer arm state"""
        payload = [track_idx, 1 if is_armed else 0]
        key = f"{CMD_MIXER_ARM}_T{track_idx}"
        self.queue_message(CMD_MIXER_ARM, payload, priority_override=2, state_key=key)

    # === SCENE HELPERS ===
    def queue_scene_name(self, scene_idx, name_bytes):
        """Queue scene name update"""
        payload = [scene_idx, len(name_bytes)]
        payload.extend(name_bytes)
        key = f"{CMD_SCENE_NAME}_S{scene_idx}"
        self.queue_message(CMD_SCENE_NAME, payload, priority_override=3, state_key=key)

    def queue_scene_color(self, scene_idx, r, g, b):
        """Queue scene color update"""
        payload = [scene_idx, r, g, b]
        key = f"{CMD_SCENE_COLOR}_S{scene_idx}"
        self.queue_message(CMD_SCENE_COLOR, payload, priority_override=3, state_key=key)
    
    def _schedule_flush(self):
        """Schedule message flush based on frame rate"""
        try:
            current_time = time.time() * 1000
            
            # Cancel existing timer
            if self._flush_timer:
                self._flush_timer.cancel()
            
            # Calculate when to flush
            time_since_last_send = current_time - self._last_send_time
            
            if time_since_last_send >= self.frame_time_ms:
                # Enough time has passed - flush immediately
                self._flush_messages()
            else:
                # Schedule flush at next frame boundary
                delay_ms = self.frame_time_ms - time_since_last_send
                self._flush_timer = Timer(delay_ms / 1000.0, self._flush_messages)
                self._flush_timer.start()
                
        except Exception as e:
            self.c_surface.log_message(f"❌ Error scheduling flush: {e}")
    
    def force_flush(self):
        """Force immediate flush of all pending messages (ignores rate limit)"""
        try:
            if not self._pending_messages:
                self.c_surface.log_message("⚠️ force_flush: No pending messages")
                return

            num_msgs = len(self._pending_messages)
            self.c_surface.log_message(f"🚀 FORCE FLUSH: Sending {num_msgs} pending messages immediately")

            current_time = time.time() * 1000

            # Sort messages by priority (lower number = higher priority)
            messages = list(self._pending_messages.values())
            messages.sort(key=lambda x: x['priority'])

            # Count message types
            track_color_count = sum(1 for m in messages if m['command'] == 0x11)  # CMD_TRACK_COLOR
            track_name_count = sum(1 for m in messages if m['command'] == 0x10)   # CMD_TRACK_NAME
            clip_count = sum(1 for m in messages if m['command'] == 0x05)         # CMD_CLIP_STATE

            self.c_surface.log_message(f"  Track colors: {track_color_count}, Track names: {track_name_count}, Clips: {clip_count}")

            # Group messages into batches for efficient sending
            led_batch = []
            control_batch = []

            for msg in messages:
                if self._is_led_command(msg['command']):
                    led_batch.append(msg)
                else:
                    control_batch.append(msg)

            # Send control messages first (higher priority)
            for msg in control_batch:
                self._send_single_message(msg)

            # Send LED updates in optimized batches
            self._send_led_batch(led_batch)

            # Update statistics
            self._messages_sent += len(messages)
            self._last_send_time = current_time

            # Clear pending messages
            self._pending_messages.clear()

            self.c_surface.log_message(f"✅ FORCE FLUSH complete: {num_msgs} messages sent")

        except Exception as e:
            self.c_surface.log_message(f"❌ Error force flushing messages: {e}")

    def _flush_messages(self):
        """Flush all pending messages to hardware"""
        try:
            if not self._pending_messages:
                return

            current_time = time.time() * 1000

            # Check frame rate limit
            time_since_last = current_time - self._last_send_time
            if time_since_last < self.frame_time_ms:
                self._frames_dropped += 1
                return  # Drop this frame to maintain 60fps limit

            # Sort messages by priority (lower number = higher priority)
            messages = list(self._pending_messages.values())
            messages.sort(key=lambda x: x['priority'])

            # Group messages into batches for efficient sending
            led_batch = []
            control_batch = []

            for msg in messages:
                if self._is_led_command(msg['command']):
                    led_batch.append(msg)
                else:
                    control_batch.append(msg)

            # Send control messages first (higher priority)
            for msg in control_batch:
                self._send_single_message(msg)

            # Send LED updates in optimized batches
            self._send_led_batch(led_batch)

            # Update statistics
            self._messages_sent += len(messages)
            self._last_send_time = current_time

            # Clear pending messages
            self._pending_messages.clear()

            # Log performance stats occasionally
            if self._messages_sent % 100 == 0:
                self._log_performance_stats()

        except Exception as e:
            self.c_surface.log_message(f"❌ Error flushing messages: {e}")
    
    def _send_single_message(self, message_data):
        """Send a single message"""
        try:
            command = message_data['command']
            payload = message_data['payload']
            
            # Create and send SysEx
            sysex_message = SysExEncoder.create_sysex(command, payload)
            if sysex_message:
                self.c_surface._send_midi(tuple(sysex_message))
                
                # Store state for duplicate detection
                state_key = message_data.get('state_key', command)
                self._last_states[state_key] = payload.copy() if hasattr(payload, 'copy') else list(payload)
                
        except Exception as e:
            self.c_surface.log_message(f"❌ Error sending message 0x{message_data['command']:02X}: {e}")
    
    def _send_led_batch(self, led_messages):
        """Send LED messages in optimized batches"""
        try:
            if not led_messages:
                return
            
            # For LED updates, we can send them in sequence quickly
            # since they're just visual updates
            for msg in led_messages:
                self._send_single_message(msg)
                
            # Could implement frame compression here:
            # - Combine multiple grid updates into single message
            # - Use delta compression for partial updates
            # - Skip updates for LEDs that haven't changed
            
        except Exception as e:
            self.c_surface.log_message(f"❌ Error sending LED batch: {e}")
    
    def _log_performance_stats(self):
        """Log performance statistics"""
        try:
            coalesce_ratio = (self._messages_coalesced / max(1, self._messages_sent)) * 100
            
            self.c_surface.log_message(
                f"📊 Message Performance: "
                f"Coalesced={self._messages_coalesced}, "
                f"Sent={self._messages_sent}, "
                f"Ratio={coalesce_ratio:.1f}%, "
                f"Dropped={self._frames_dropped}"
            )
            
        except Exception as e:
            self.c_surface.log_message(f"❌ Error logging stats: {e}")
    
    def force_flush(self):
        """Force immediate flush of all pending messages"""
        try:
            if self._flush_timer:
                self._flush_timer.cancel()
                self._flush_timer = None
            
            self._flush_messages()
            
        except Exception as e:
            self.c_surface.log_message(f"❌ Error forcing flush: {e}")
    
    def set_frame_rate(self, fps):
        """Set target frame rate"""
        try:
            self.target_fps = max(1, min(120, fps))  # Clamp 1-120fps
            self.frame_time_ms = 1000.0 / self.target_fps
            
            self.c_surface.log_message(f"🎯 Message coalescer frame rate: {self.target_fps}fps")
            
        except Exception as e:
            self.c_surface.log_message(f"❌ Error setting frame rate: {e}")
    
    def get_performance_info(self):
        """Get performance information"""
        try:
            return {
                'target_fps': self.target_fps,
                'frame_time_ms': self.frame_time_ms,
                'coalesce_window_ms': self.coalesce_window_ms,
                'pending_messages': len(self._pending_messages),
                'messages_coalesced': self._messages_coalesced,
                'messages_sent': self._messages_sent,
                'frames_dropped': self._frames_dropped,
                'coalesce_ratio': (self._messages_coalesced / max(1, self._messages_sent)) * 100
            }
            
        except Exception as e:
            self.c_surface.log_message(f"❌ Error getting performance info: {e}")
            return {}
    
    def cleanup(self):
        """Cleanup coalescer resources"""
        try:
            # Cancel any pending timers
            if self._flush_timer:
                self._flush_timer.cancel()
                self._flush_timer = None
            
            if self._frame_timer:
                self._frame_timer.cancel()
                self._frame_timer = None
            
            # Force send any remaining messages
            self.force_flush()
            
            # Clear state
            self._pending_messages.clear()
            self._last_states.clear()
            
            self.c_surface.log_message("✅ Message coalescer cleaned up")
            
        except Exception as e:
            self.c_surface.log_message(f"❌ Error cleaning up coalescer: {e}")
    
    # ========================================
    # FRAME COMPRESSION METHODS (Future)
    # ========================================
    
    def _create_grid_delta(self, command, new_grid, old_grid):
        """Create delta update for grid changes (future optimization)"""
        try:
            if not old_grid or len(new_grid) != len(old_grid):
                return new_grid  # Send full frame
            
            # Find changed positions
            changes = []
            for i, (new_val, old_val) in enumerate(zip(new_grid, old_grid)):
                if new_val != old_val:
                    changes.append((i, new_val))
            
            # If too many changes, send full frame
            if len(changes) > len(new_grid) // 2:
                return new_grid
            
            # Create delta message
            delta_payload = []
            for pos, val in changes:
                delta_payload.extend([pos, val])
            
            return delta_payload
            
        except Exception:
            return new_grid  # Fallback to full frame
    
    def _compress_led_frame(self, messages):
        """Compress multiple LED updates into single frame (future)"""
        try:
            # Group by grid type
            grids = {}
            for msg in messages:
                command = msg['command']
                if command in grids:
                    grids[command] = msg['payload']  # Keep latest
                else:
                    grids[command] = msg['payload']
            
            # Could implement:
            # - Run-length encoding for sparse grids
            # - Delta compression between frames
            # - Smart batching of related updates
            
            return list(grids.items())
            
        except Exception:
            return [(msg['command'], msg['payload']) for msg in messages]
