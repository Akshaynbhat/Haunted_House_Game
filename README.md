# 🏚️ 3D Haunted House

A 3D horror-themed house environment developed using **C++, OpenGL, and FreeGLUT** as a Computer Graphics group project. The project demonstrates 3D modelling, transformations, lighting, camera movement, keyboard interaction, and basic horror effects.

## 📌 Project Overview

The 3D Haunted House allows users to explore a dark, spooky environment with a haunted house, trees, a moonlit sky, flickering lights, and a moving ghost. Users can navigate using the keyboard, operate the front door, and toggle a flashlight to explore the surroundings.

## ✨ Features

- **3D Haunted House:** House structure with walls, roof, windows, and front door.
- **Environment:** Ground, moon, stars, and trees.
- **Dark Atmosphere:** Ambient lighting and coloured light effects.
- **Flickering Lights:** Simulates an unstable light source.
- **Moving Ghost:** A ghost moves around the haunted environment.
- **Flashlight:** Toggle the flashlight to illuminate the surroundings.
- **Interactive Door:** Open or close the front door when nearby.
- **Camera Controls:** Move and look around the environment using the keyboard.
- **On-Screen Help:** Display keyboard controls and status information.
- **Reset Function:** Reset the player's position and view.

## 🎮 Keyboard Controls

| Key | Function |
|---|---|
| `W` | Move forward |
| `A` | Move left |
| `S` | Move backward |
| `D` | Move right |
| `↑` | Look up |
| `↓` | Look down |
| `←` | Look left |
| `→` | Look right |
| `F` | Toggle flashlight |
| `E` | Open or close the door when nearby |
| `H` | Show or hide help |
| `R` | Reset position and view |
| `Q` or `Esc` | Exit the application |

## 🛠️ Technologies Used

- **Programming Language:** C++
- **Graphics API:** OpenGL
- **Window and Input Management:** FreeGLUT
- **Compiler:** GCC or Microsoft Visual C++ (MSVC)
- **Development Environment:** Visual Studio Community or MSYS2 UCRT64

All the core development tools used by this project are available free of charge.

## 💻 System Requirements

- Windows operating system
- C++ compiler
- OpenGL-compatible graphics support
- FreeGLUT library
- Visual Studio Community with the C++ desktop development workload, or MSYS2 UCRT64 with GCC and FreeGLUT

## 🚀 Installation and Execution

### Option 1: Using MSYS2 UCRT64

**Step 1: Install the required packages**

Open the **MSYS2 UCRT64** terminal and run:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-freeglut
```

**Step 2: Navigate to the project folder**

Change the directory to the folder containing the source code. For example:

```bash
cd /c/Users/YourUsername/Projects/HauntedHouse
```

Replace the example path with your actual project folder path.

**Step 3: Compile the program**

```bash
g++ haunted_house_keyboard_interaction.cpp -o haunted.exe -lfreeglut -lopengl32 -lglu32
```

**Step 4: Run the application**

```bash
./haunted.exe
```

### Option 2: Using Visual Studio Community

1. Install the **Desktop development with C++** workload.
2. Install and configure FreeGLUT using `vcpkg`.
3. Open the C++ project in Visual Studio.
4. Ensure the project platform is set to **x64**.
5. Build the project using **Build Solution**.
6. Run the application using **Start Without Debugging** (`Ctrl + F5`).

Do not mix MSYS2 GCC libraries with MSVC libraries. Use the compiler and FreeGLUT installation that match your chosen environment.

## 📁 Project Structure

```text
3D-Haunted-House/
│
├── haunted_house_keyboard_interaction.cpp
└── README.md
```

The project currently uses a single C++ source file to keep the implementation simple and easy to build.

## 📚 Computer Graphics Concepts Demonstrated

- 3D modelling using geometric primitives
- Perspective projection
- Camera positioning and viewing transformations
- Translation, rotation, and scaling
- OpenGL lighting and material properties
- Keyboard event handling
- Animation and time-based updates
- Depth testing and double buffering
- Real-time rendering

## ⚠️ Current Limitations

This is an early prototype of the haunted house project. Some planned gameplay features are not yet implemented.

- Player collision detection is limited.
- The ghost does not yet chase the player.
- Key collection and locked-room mechanics are not implemented.
- The escape mission and game-over system are planned enhancements.
- Mouse-based camera control is not currently included.

## 🔮 Future Enhancements

- Add player collision detection and prevent movement through walls.
- Implement keys, locked rooms, and collectible clues.
- Create an escape mission with win and game-over conditions.
- Make the ghost chase the player.
- Add background horror sounds and sound effects.
- Improve textures, models, and lighting.
- Add mouse-based camera movement.
- Create a main menu, pause screen, and restart option.
- Add a timer and scoring system.

## 👥 Team Contributions

The project can be divided among three team members:

| Team Member | Responsibility |
|---|---|
| Member 1 | House modelling, environment, trees, and scenery |
| Member 2 | Camera controls, movement, and keyboard input |
| Member 3 | Interactive objects, lighting effects, ghost animation, and game logic |

Update this section with the actual names and contributions of the team members.

## 🧪 Testing

The following checks can be performed:

- Verify that the house and environment render correctly.
- Test movement using `W`, `A`, `S`, and `D`.
- Test camera rotation using the arrow keys.
- Verify that the flashlight toggles using `F`.
- Test the door interaction using `E` near the entrance.
- Verify that the help overlay toggles using `H`.
- Test position reset using `R`.
- Confirm that `Q` or `Esc` closes the application.

## 📄 License

This project was developed for educational purposes as a Computer Graphics academic project. Add an explicit open-source license if you intend to distribute the source code for reuse.

---

**Project:** 3D Haunted House  
**Language:** C++  
**Graphics Library:** OpenGL and FreeGLUT  
**Purpose:** Computer Graphics academic group project
