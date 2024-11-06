import matplotlib
import matplotlib.pyplot as plt
import matplotlib.animation

def make_playback_animation(savepath, spec, duration_ms, vmin=20, vmax=90):
    fig, axs = plt.subplots()
    axs.set_axis_off()
    fig.set_size_inches((duration_ms / 1000 * 5, 5))
    frames = []
    frame_duration=20
    num_frames = int(duration_ms / frame_duration + .99)

    spec_height, spec_width = spec.shape
    for i in range(num_frames):
        xpos = (i - 1) / (num_frames - 3) * (spec_width - 1)
        new_frame = axs.imshow(spec, cmap='inferno', origin='lower', aspect='auto', vmin=vmin, vmax=vmax)
        if i in {0, num_frames - 1}:
            frames.append([new_frame])
        else:
            line = axs.plot([xpos, xpos], [0, spec_height-1], color='white', alpha=0.8)[0]
            frames.append([new_frame, line])


    ani = matplotlib.animation.ArtistAnimation(fig, frames, blit=True, interval=frame_duration)
    ani.save(savepath, dpi=720)