# Use a specific Ubuntu Long-Term Support (LTS) release for reproducibility
FROM ubuntu:22.04

# Set non-interactive mode for package installations to avoid prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install all build dependencies
RUN apt-get update && apt-get install -y \
    # --- Basic build utilities ---
    build-essential \  
    git \               
    pkg-config \        
    wget \              
    curl \              
    vim \              
    # --- QEMU Build System Dependencies ---
    meson \             
    # QEMU uses the Meson build system
    ninja-build \       
    # Used by Meson for fast parallel builds
    python3 \           
    # Required by build scripts
    python3-pip \       
    # --- QEMU Core Library Dependencies ---
    libglib2.0-dev \    
    # GLib development files (core dependency)
    libpixman-1-dev \   
    # Pixman library (graphics manipulation)
    libfdt-dev \        
    # Flattened Device Tree library
    libslirp-dev \      
    # User-mode networking (libslirp)
    # --- Optional (but common) dependencies ---
    # Add these if you need specific features like a graphical UI
    # libsdl2-dev \       # For SDL-based UI
    # libgtk-3-dev \      # For GTK-based UI
    # libvte-2.91-dev \   # For GTK VTE terminal widget
    # libnfs-dev \        # For NFS support
    # libiscsi-dev \      # For iSCSI support
    # --- Cleanup ---
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*
# install pip dependence
RUN pip install tomli

# Create a non-root user for building (good practice)
RUN useradd -m -s /bin/bash user1
USER user1

# Set the default working directory inside the container
WORKDIR /home/user1

# Set the default command to open a shell
CMD ["/bin/bash"]