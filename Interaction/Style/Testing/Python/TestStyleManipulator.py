#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""Unit tests for vtkInteractorStyleManipulator."""

import sys

from vtkmodules.vtkInteractionStyle import (
    vtkCameraManipulator,
    vtkInteractorStyleManipulator,
    vtkTrackballPan,
    vtkTrackballRotate,
    vtkTrackballZoom,
)
from vtkmodules.vtkRenderingCore import (
    vtkRenderWindow,
    vtkRenderWindowInteractor,
    vtkRenderer,
)
import vtkmodules.vtkInteractionStyle
import vtkmodules.vtkRenderingOpenGL2
import vtkmodules.test.Testing


class TestStyleManipulator(vtkmodules.test.Testing.vtkTest):
    """Tests for vtkInteractorStyleManipulator."""

    def setUp(self):
        self.style = vtkInteractorStyleManipulator()
        self.renderer = vtkRenderer()
        self.render_window = vtkRenderWindow()
        self.render_window.SetSize(300, 300)
        self.render_window.AddRenderer(self.renderer)
        self.interactor = vtkRenderWindowInteractor()
        self.interactor.SetRenderWindow(self.render_window)
        self.interactor.SetInteractorStyle(self.style)

    def test_default_properties(self):
        """Test default values of properties."""
        style = vtkInteractorStyleManipulator()
        self.assertEqual(style.GetCenterOfRotation(), (0.0, 0.0, 0.0))
        self.assertEqual(style.GetMouseMotionFactor(), 1.0)
        self.assertFalse(style.GetMouseWheelZoomsToCursor())
        self.assertIsNotNone(style.GetCameraManipulators())
        self.assertEqual(style.GetCameraManipulators().GetNumberOfItems(), 0)

    def test_center_of_rotation(self):
        """Test setting and getting center of rotation."""
        style = vtkInteractorStyleManipulator()
        style.SetCenterOfRotation(1.0, 2.0, 3.0)
        self.assertEqual(style.GetCenterOfRotation(), (1.0, 2.0, 3.0))

    def test_rotation_factor(self):
        """Test setting and getting rotation factor."""
        style = vtkInteractorStyleManipulator()
        style.SetMouseMotionFactor(2.5)
        self.assertEqual(style.GetMouseMotionFactor(), 2.5)

    def test_mouse_wheel_zooms_to_cursor(self):
        """Test setting and getting MouseWheelZoomsToCursor."""
        style = vtkInteractorStyleManipulator()
        style.SetMouseWheelZoomsToCursor(True)
        self.assertTrue(style.GetMouseWheelZoomsToCursor())
        style.SetMouseWheelZoomsToCursor(False)
        self.assertFalse(style.GetMouseWheelZoomsToCursor())

    def test_add_manipulator(self):
        """Test adding manipulators."""
        style = vtkInteractorStyleManipulator()

        rotate = vtkTrackballRotate()
        rotate.SetMouseButton(vtkCameraManipulator.MouseButtonType.Left)
        style.AddManipulator(rotate)
        self.assertEqual(style.GetCameraManipulators().GetNumberOfItems(), 1)

        pan = vtkTrackballPan()
        pan.SetMouseButton(vtkCameraManipulator.MouseButtonType.Middle)
        style.AddManipulator(pan)
        self.assertEqual(style.GetCameraManipulators().GetNumberOfItems(), 2)

        zoom = vtkTrackballZoom()
        zoom.SetMouseButton(vtkCameraManipulator.MouseButtonType.Right)
        style.AddManipulator(zoom)
        self.assertEqual(style.GetCameraManipulators().GetNumberOfItems(), 3)

    def test_remove_all_manipulators(self):
        """Test removing all manipulators."""
        style = vtkInteractorStyleManipulator()

        rotate = vtkTrackballRotate()
        rotate.SetMouseButton(vtkCameraManipulator.MouseButtonType.Left)
        style.AddManipulator(rotate)

        pan = vtkTrackballPan()
        pan.SetMouseButton(vtkCameraManipulator.MouseButtonType.Middle)
        style.AddManipulator(pan)

        self.assertEqual(style.GetCameraManipulators().GetNumberOfItems(), 2)
        style.RemoveAllManipulators()
        self.assertEqual(style.GetCameraManipulators().GetNumberOfItems(), 0)

    def test_find_manipulator_by_button(self):
        """Test finding manipulators by button number."""
        style = vtkInteractorStyleManipulator()

        rotate = vtkTrackballRotate()
        rotate.SetMouseButton(vtkCameraManipulator.MouseButtonType.Left)
        style.AddManipulator(rotate)

        pan = vtkTrackballPan()
        pan.SetMouseButton(vtkCameraManipulator.MouseButtonType.Middle)
        style.AddManipulator(pan)

        zoom = vtkTrackballZoom()
        zoom.SetMouseButton(vtkCameraManipulator.MouseButtonType.Right)
        style.AddManipulator(zoom)

        found = style.FindManipulator(vtkCameraManipulator.MouseButtonType.Left, False, False, False)
        self.assertIsNotNone(found)
        self.assertEqual(found, rotate)

        found = style.FindManipulator(vtkCameraManipulator.MouseButtonType.Middle, False, False, False)
        self.assertIsNotNone(found)
        self.assertEqual(found, pan)

        found = style.FindManipulator(vtkCameraManipulator.MouseButtonType.Right, False, False, False)
        self.assertIsNotNone(found)
        self.assertEqual(found, zoom)

    def test_find_manipulator_with_modifiers(self):
        """Test finding manipulators with modifier keys."""
        style = vtkInteractorStyleManipulator()

        # Left button, no modifiers -> rotate
        rotate = vtkTrackballRotate()
        rotate.SetMouseButton(vtkCameraManipulator.MouseButtonType.Left)
        style.AddManipulator(rotate)

        # Left button + control -> pan
        pan = vtkTrackballPan()
        pan.SetMouseButton(vtkCameraManipulator.MouseButtonType.Left)
        pan.SetControl(True)
        style.AddManipulator(pan)

        # Left button + shift -> zoom
        zoom = vtkTrackballZoom()
        zoom.SetMouseButton(vtkCameraManipulator.MouseButtonType.Left)
        zoom.SetShift(True)
        style.AddManipulator(zoom)

        # Middle button, no modifiers -> pan
        pan2 = vtkTrackballPan()
        pan2.SetMouseButton(vtkCameraManipulator.MouseButtonType.Middle)
        style.AddManipulator(pan2)

        # Middle button + control -> rotate
        rotate2 = vtkTrackballRotate()
        rotate2.SetMouseButton(vtkCameraManipulator.MouseButtonType.Middle)
        rotate2.SetControl(True)
        style.AddManipulator(rotate2)

        # Middle button + shift -> zoom
        zoom2 = vtkTrackballZoom()
        zoom2.SetMouseButton(vtkCameraManipulator.MouseButtonType.Middle)
        zoom2.SetShift(True)
        style.AddManipulator(zoom2)

        # Right button, no modifiers -> zoom
        zoom3 = vtkTrackballZoom()
        zoom3.SetMouseButton(vtkCameraManipulator.MouseButtonType.Right)
        style.AddManipulator(zoom3)

        # Right button + control -> pan
        pan3 = vtkTrackballPan()
        pan3.SetMouseButton(vtkCameraManipulator.MouseButtonType.Right)
        pan3.SetControl(True)
        style.AddManipulator(pan3)

        # Right button + shift -> rotate
        rotate3 = vtkTrackballRotate()
        rotate3.SetMouseButton(vtkCameraManipulator.MouseButtonType.Right)
        rotate3.SetShift(True)
        style.AddManipulator(rotate3)

        # Find rotate: button=1, no modifiers
        found = style.FindManipulator(vtkCameraManipulator.MouseButtonType.Left, False, False, False)
        self.assertEqual(found, rotate)

        # Find pan: button=1, control
        found = style.FindManipulator(vtkCameraManipulator.MouseButtonType.Left, False, True, False)
        self.assertEqual(found, pan)

        # Find zoom: button=1, shift
        found = style.FindManipulator(vtkCameraManipulator.MouseButtonType.Left, False, False, True)
        self.assertEqual(found, zoom)

        # Find pan2: button=2, no modifiers
        found = style.FindManipulator(vtkCameraManipulator.MouseButtonType.Middle, False, False, False)
        self.assertEqual(found, pan2)

        # Find rotate2: button=2, shift
        found = style.FindManipulator(vtkCameraManipulator.MouseButtonType.Middle, False, True, False)
        self.assertEqual(found, rotate2)

        # Find zoom2: button=2, control
        found = style.FindManipulator(vtkCameraManipulator.MouseButtonType.Middle, False, False, True)
        self.assertEqual(found, zoom2)

        # Find zoom3: button=3, no modifiers
        found = style.FindManipulator(vtkCameraManipulator.MouseButtonType.Right, False, False, False)
        self.assertEqual(found, zoom3)

        # Find pan3: button=3, shift
        found = style.FindManipulator(vtkCameraManipulator.MouseButtonType.Right, False, True, False)
        self.assertEqual(found, pan3)

        # Find rotate3: button=3, control
        found = style.FindManipulator(vtkCameraManipulator.MouseButtonType.Right, False, False, True)
        self.assertEqual(found, rotate3)

    def test_find_manipulator_returns_none(self):
        """Test that FindManipulator returns None when no match is found."""
        style = vtkInteractorStyleManipulator()

        rotate = vtkTrackballRotate()
        rotate.SetMouseButton(vtkCameraManipulator.MouseButtonType.Left)
        style.AddManipulator(rotate)

        # Button 2 has no manipulator registered
        found = style.FindManipulator(vtkCameraManipulator.MouseButtonType.Middle, False, False, False)
        self.assertIsNone(found)

        # Button 1 + shift has no manipulator registered
        found = style.FindManipulator(vtkCameraManipulator.MouseButtonType.Left, False, False, True)
        self.assertIsNone(found)

    def test_interaction_events(self):
        """Test that interaction events are fired during button press/release."""
        style = self.style

        rotate = vtkTrackballRotate()
        rotate.SetMouseButton(vtkCameraManipulator.MouseButtonType.Left)
        style.AddManipulator(rotate)

        start_count = [0]
        end_count = [0]

        def on_start(obj, event):
            start_count[0] += 1

        def on_end(obj, event):
            end_count[0] += 1

        style.AddObserver("StartInteractionEvent", on_start)
        style.AddObserver("EndInteractionEvent", on_end)

        self.render_window.Render()

        # Simulate left button press
        self.interactor.SetEventInformationFlipY(150, 150, 0, 0, '\0', 0, '')
        self.interactor.InvokeEvent("LeftButtonPressEvent")
        self.assertEqual(start_count[0], 1, "StartInteractionEvent should have fired once when left button was pressed")

        # Simulate left button release
        self.interactor.SetEventInformationFlipY(160, 160, 0, 0, '\0', 0, '')
        self.interactor.InvokeEvent("LeftButtonReleaseEvent")
        self.assertEqual(end_count[0], 1, "EndInteractionEvent should have fired once when left button was released")

    def test_mouse_move_during_interaction(self):
        """Test that mouse move fires InteractionEvent when a manipulator is active."""
        style = self.style

        rotate = vtkTrackballRotate()
        rotate.SetMouseButton(vtkCameraManipulator.MouseButtonType.Left)
        style.AddManipulator(rotate)

        interaction_count = [0]

        def on_interaction(obj, event):
            interaction_count[0] += 1

        style.AddObserver("InteractionEvent", on_interaction)

        self.render_window.Render()

        # Press left button to start interaction
        self.interactor.SetEventInformationFlipY(150, 150, 0, 0, '\0', 0, '')
        self.interactor.InvokeEvent("LeftButtonPressEvent")

        # Move the mouse
        self.interactor.SetEventInformationFlipY(160, 160, 0, 0, '\0', 0, '')
        self.interactor.InvokeEvent("MouseMoveEvent")
        self.assertGreater(interaction_count[0], 0, "InteractionEvent should have fired at least once during mouse move with left button pressed")

        # Release to end interaction
        self.interactor.InvokeEvent("LeftButtonReleaseEvent")

    def test_no_double_button_press(self):
        """Test that pressing a second button while one is active is ignored."""
        style = self.style

        rotate = vtkTrackballRotate()
        rotate.SetMouseButton(vtkCameraManipulator.MouseButtonType.Left)
        style.AddManipulator(rotate)

        pan = vtkTrackballPan()
        pan.SetMouseButton(vtkCameraManipulator.MouseButtonType.Right)
        style.AddManipulator(pan)

        start_count = [0]

        def on_start(obj, event):
            start_count[0] += 1

        style.AddObserver("StartInteractionEvent", on_start)

        self.render_window.Render()

        # Press left button
        self.interactor.SetEventInformationFlipY(150, 150, 0, 0, '\0', 0, '')
        self.interactor.InvokeEvent("LeftButtonPressEvent")
        self.assertEqual(start_count[0], 1, "StartInteractionEvent should have fired once when left button was pressed")

        # Press right button while left is still down - should be ignored
        self.interactor.SetEventInformationFlipY(150, 150, 0, 0, '\0', 0, '')
        self.interactor.InvokeEvent("RightButtonPressEvent")
        self.assertEqual(start_count[0], 1, "StartInteractionEvent should not have fired again when right button was pressed while left button is still down")

        # Release left button
        self.interactor.SetEventInformationFlipY(150, 150, 0, 0, '\0', 0, '')
        self.interactor.InvokeEvent("LeftButtonReleaseEvent")

    def test_wrong_button_release_ignored(self):
        """Test that releasing a different button than was pressed is ignored."""
        style = self.style

        rotate = vtkTrackballRotate()
        rotate.SetMouseButton(vtkCameraManipulator.MouseButtonType.Left)
        style.AddManipulator(rotate)

        end_count = [0]

        def on_end(obj, event):
            end_count[0] += 1

        style.AddObserver("EndInteractionEvent", on_end)

        self.render_window.Render()

        # Press left button
        self.interactor.SetEventInformationFlipY(150, 150, 0, 0, '\0', 0, '')
        self.interactor.InvokeEvent("LeftButtonPressEvent")

        # Release right button - should be ignored since left was pressed
        self.interactor.SetEventInformationFlipY(150, 150, 0, 0, '\0', 0, '')
        self.interactor.InvokeEvent("RightButtonReleaseEvent")
        self.assertEqual(end_count[0], 0, "EndInteractionEvent should not have fired when right button was released while left button was actively held down")

        # Release left button - should actually end interaction
        self.interactor.SetEventInformationFlipY(150, 150, 0, 0, '\0', 0, '')
        self.interactor.InvokeEvent("LeftButtonReleaseEvent")
        self.assertEqual(end_count[0], 1, "EndInteractionEvent should have fired once when left button was released")

    def test_middle_button_interaction(self):
        """Test interaction with middle button."""
        style = self.style

        pan = vtkTrackballPan()
        pan.SetMouseButton(vtkCameraManipulator.MouseButtonType.Middle)
        style.AddManipulator(pan)

        start_count = [0]
        end_count = [0]

        def on_start(obj, event):
            start_count[0] += 1

        def on_end(obj, event):
            end_count[0] += 1

        style.AddObserver("StartInteractionEvent", on_start)
        style.AddObserver("EndInteractionEvent", on_end)

        self.render_window.Render()

        self.interactor.SetEventInformationFlipY(150, 150, 0, 0, '\0', 0, '')
        self.interactor.InvokeEvent("MiddleButtonPressEvent")
        self.assertEqual(start_count[0], 1, "StartInteractionEvent should have fired once when middle button was pressed")

        self.interactor.SetEventInformationFlipY(160, 160, 0, 0, '\0', 0, '')
        self.interactor.InvokeEvent("MiddleButtonReleaseEvent")
        self.assertEqual(end_count[0], 1, "EndInteractionEvent should have fired once when middle button was released")

    def test_shift_modifier_interaction(self):
        """Test interaction with shift modifier key."""
        style = self.style

        zoom = vtkTrackballZoom()
        zoom.SetMouseButton(vtkCameraManipulator.MouseButtonType.Left)
        zoom.SetShift(True)
        style.AddManipulator(zoom)

        start_count = [0]

        def on_start(obj, event):
            start_count[0] += 1

        style.AddObserver("StartInteractionEvent", on_start)

        self.render_window.Render()

        # Press left button with shift held
        self.interactor.SetEventInformationFlipY(150, 150, 0, 1, '\0', 0, '')
        self.interactor.InvokeEvent("LeftButtonPressEvent")
        self.assertEqual(start_count[0], 1, "StartInteractionEvent should have fired once when left button was pressed with shift modifier")

        self.interactor.SetEventInformationFlipY(150, 150, 0, 1, '\0', 0, '')
        self.interactor.InvokeEvent("LeftButtonReleaseEvent")

    def test_typical_three_button_setup(self):
        """Test a typical 3-button mouse configuration with rotate/pan/zoom."""
        style = self.style

        # Left click = rotate
        rotate = vtkTrackballRotate()
        rotate.SetMouseButton(vtkCameraManipulator.MouseButtonType.Left)
        style.AddManipulator(rotate)

        # Middle click = pan
        pan = vtkTrackballPan()
        pan.SetMouseButton(vtkCameraManipulator.MouseButtonType.Middle)
        style.AddManipulator(pan)

        # Right click = zoom
        zoom = vtkTrackballZoom()
        zoom.SetMouseButton(vtkCameraManipulator.MouseButtonType.Right)
        style.AddManipulator(zoom)

        # Verify each manipulator is found
        self.assertEqual(style.FindManipulator(vtkCameraManipulator.MouseButtonType.Left, False, False, False), rotate, "FindManipulator should return rotate manipulator for left button with no modifiers")
        self.assertEqual(style.FindManipulator(vtkCameraManipulator.MouseButtonType.Middle, False, False, False), pan, "FindManipulator should return pan manipulator for middle button with no modifiers")
        self.assertEqual(style.FindManipulator(vtkCameraManipulator.MouseButtonType.Right, False, False, False), zoom, "FindManipulator should return zoom manipulator for right button with no modifiers")

        self.render_window.Render()

        # Test full interaction cycle with each button
        for button_name in ["Left", "Middle", "Right"]:
            start_count = [0]
            end_count = [0]
            interaction_count = [0]
            def on_start(obj, event):
                start_count[0] += 1
            def on_end(obj, event):
                end_count[0] += 1
            def on_interaction(obj, event):
                interaction_count[0] += 1

            observer_tags = [style.AddObserver("StartInteractionEvent", on_start),
                             style.AddObserver("EndInteractionEvent", on_end),
                             style.AddObserver("InteractionEvent", on_interaction),
                             ]
            self.interactor.SetEventInformationFlipY(150, 150, 0, 0, '\0', 0, '')
            self.interactor.InvokeEvent(button_name + "ButtonPressEvent")
            self.assertEqual(start_count[0], 1, f"StartInteractionEvent should have fired once when {button_name} button was pressed")

            self.interactor.SetEventInformationFlipY(170, 170, 0, 0, '\0', 0, '')
            self.interactor.InvokeEvent("MouseMoveEvent")
            self.assertGreater(interaction_count[0], 0, f"InteractionEvent should have fired at least once during mouse move with {button_name} button pressed")

            self.interactor.SetEventInformationFlipY(170, 170, 0, 0, '\0', 0, '')
            self.interactor.InvokeEvent(button_name + "ButtonReleaseEvent")
            self.assertEqual(end_count[0], 1, f"EndInteractionEvent should have fired once when {button_name} button was released")

            [style.RemoveObserver(tag) for tag in observer_tags]

if __name__ == "__main__":
    vtkmodules.test.Testing.main([(TestStyleManipulator, 'test')])
