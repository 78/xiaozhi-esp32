/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_cam_sensor.h"

static const char *TAG = "cam_sensor";

esp_err_t esp_cam_sensor_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    ESP_RETURN_ON_FALSE(dev && qdesc, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(dev->ops->query_para_desc, ESP_ERR_NOT_SUPPORTED, TAG, "unsupported operation");

    return dev->ops->query_para_desc(dev, qdesc);
}

esp_err_t esp_cam_sensor_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(dev->ops->get_para_value, ESP_ERR_NOT_SUPPORTED, TAG, "unsupported operation");

    return dev->ops->get_para_value(dev, id, arg, size);
}

esp_err_t esp_cam_sensor_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(dev->ops->set_para_value, ESP_ERR_NOT_SUPPORTED, TAG, "unsupported operation");

    return dev->ops->set_para_value(dev, id, arg, size);
}

esp_err_t esp_cam_sensor_get_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *caps)
{
    ESP_RETURN_ON_FALSE(dev && caps, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(dev->ops->query_support_capability, ESP_ERR_NOT_SUPPORTED, TAG, "unsupported operation");

    return dev->ops->query_support_capability(dev, caps);
}

esp_err_t esp_cam_sensor_query_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *format_arry)
{
    ESP_RETURN_ON_FALSE(dev && format_arry, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(dev->ops->query_support_formats, ESP_ERR_NOT_SUPPORTED, TAG, "unsupported operation");

    return dev->ops->query_support_formats(dev, format_arry);
}

esp_err_t esp_cam_sensor_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    return dev->ops->set_format(dev, format);
}

esp_err_t esp_cam_sensor_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
{
    ESP_RETURN_ON_FALSE(dev && format, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    return dev->ops->get_format(dev, format);
}

esp_err_t esp_cam_sensor_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    return dev->ops->priv_ioctl(dev, cmd, arg);
}

const char *esp_cam_sensor_get_name(esp_cam_sensor_device_t *dev)
{
    ESP_RETURN_ON_FALSE(dev, NULL, TAG, "invalid argument");

    return dev->name;
}

esp_err_t esp_cam_sensor_del_dev(esp_cam_sensor_device_t *dev)
{
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(dev->ops->del, ESP_ERR_NOT_SUPPORTED, TAG, "unsupported operation");
    return dev->ops->del(dev);
}
