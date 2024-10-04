import pytest
from cliente.checksum import checksum


def test_checksum():
    example = b"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Quisque tempor interdum purus, eget sollicitudin arcu porttitor sed. Duis egestas lectus nec justo ullamcorper, quis tempor mauris semper. Fusce in justo a tellus imperdiet fermentum et id dui. Vivamus porta diam leo, in vulputate orci molestie nec. Mauris luctus odio non tortor luctus laoreet. Proin quis augue nec mauris auctor eleifend at ultrices ipsum. In ligula purus, tincidunt eu luctus id, sollicitudin id lectus. Sed vel purus id tortor vehicula accumsan. Vivamus ut erat id libero pretium maximus et at elit. Pellentesque ultricies nulla et sagittis rutrum. Mauris sapien est, pulvinar sed dui at, placerat efficitur erat. Fusce fermentum justo id odio ultrices tempus. Nullam commodo orci eleifend nisi cursus pretium. Sed condimentum bibendum tellus, facilisis aliquam diam gravida quis. Aenean eu cursus mi. Aliquam a rutrum elit. Nunc sed lectus est. Suspendisse ac rhoncus nunc, sit amet pharetra est. Mauris aliquet libero eu arcu elementum, vel egestas lectus varius. Sed eu ultrices risus. In tempus rutrum rutrum. Duis sodales id leo in rutrum. In hac habitasse platea dictumst. Duis porttitor nulla et leo imperdiet iaculis. Ut nisl massa, vehicula ac velit eget, sodales convallis enim. Vivamus libero ante, tempus quis tempus vitae, ullamcorper eu leo. In sit amet mauris volutpat felis luctus tincidunt a non lacus."
    assert checksum(example) == 131637
    # a = 4294967295
    # 131636
