FROM nvcr.io/nvidia/pytorch:23.10-py3

USER root

ENV TZ=Asia/Tokyo
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt update

# server のダウンロード
RUN git clone https://github.com/digitalcurling/DigitalCurling3-Server.git
WORKDIR /workspace/DigitalCurling3-Server
RUN git submodule update --init --recursive

# client のダウンロード
WORKDIR /workspace
RUN git clone https://github.com/digitalcurling/DigitalCurling3-ClientExamples.git
WORKDIR /workspace/DigitalCurling3-ClientExamples
RUN git submodule update --init --recursive

# LightZero のダウンロード
WORKDIR /workspace
RUN git clone https://github.com/opendilab/LightZero.git

# Boost install
WORKDIR /workspace
RUN wget https://boostorg.jfrog.io/artifactory/main/release/1.80.0/source/boost_1_80_0.tar.gz
RUN tar xvf boost_1_80_0.tar.gz
WORKDIR /workspace/boost_1_80_0
RUN ./bootstrap.sh --prefix=/usr/
RUN ./b2 install

# build server
WORKDIR /workspace/DigitalCurling3-Server
RUN mkdir build
WORKDIR /workspace/DigitalCurling3-Server/build
RUN cmake -DCMAKE_BUILD_TYPE=Release ..
RUN cmake --build . --config Release
COPY config.json /workspace/DigitalCurling3-Server/build

# build client
COPY stdio.cpp /workspace/DigitalCurling3-ClientExamples/stdio/
WORKDIR /workspace/DigitalCurling3-ClientExamples
RUN mkdir build
WORKDIR /workspace/DigitalCurling3-ClientExamples/build
RUN cmake -DCMAKE_BUILD_TYPE=Release ..
RUN cmake --build . --config Release

# pip install
RUN python3 -m pip install --upgrade pip
WORKDIR /workspace/LightZero
RUN python3 -m pip install -e .
RUN mkdir /code
WORKDIR /code
COPY requirements.txt /code/
RUN python3 -m pip install -r requirements.txt
COPY . /code/

COPY example.py /workspace/
